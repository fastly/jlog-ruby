#include <ruby.h>
#include <jlog.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/time.h>

typedef struct {
   jlog_ctx *ctx;
   char *path;
   jlog_id start;
   jlog_id last;
   jlog_id prev;
   jlog_id end;
   int auto_checkpoint;
   int error;
} jlog_obj;

typedef jlog_obj * Jlog;
typedef jlog_obj * Jlog_Writer;
typedef jlog_obj * Jlog_Reader;

VALUE cJlog;
VALUE cJlogWriter;
VALUE cJlogReader;
VALUE eJlog;

void rJlog_populate_subscribers(VALUE);

void rJlog_mark(Jlog jo) { }

void rJlog_free(Jlog jo) {
   if(jo->ctx) {
      jlog_ctx_close(jo->ctx);
   }

   rb_warning("(rJlog_free) Freeing path");
   if(jo->path) {
      xfree(jo->path);
   }

   if(jo) {
     rb_warning("(rJlog_free) Freeing jo");
     xfree(jo);
   }
}

void rJlog_raise(Jlog jo, const char* mess)
{
   VALUE e = rb_exc_new2(eJlog, mess);
   rb_iv_set(e, "error", INT2FIX(jlog_ctx_err(jo->ctx)));
   rb_iv_set(e, "errstr", rb_str_new2(jlog_ctx_err_string(jo->ctx)));
   rb_iv_set(e, "errno", INT2FIX(jlog_ctx_errno(jo->ctx)));

   rb_raise(eJlog, "%s: %d %s", mess, jlog_ctx_err(jo->ctx), jlog_ctx_err_string(jo->ctx));
}

VALUE rJlog_initialize(int argc, VALUE* argv, VALUE klass) 
{
   int options;
   Jlog jo;
   jlog_id zeroed = {0,0};
   VALUE path;
   VALUE optarg;
   VALUE size;

   rb_warning("(init) Calling scan args");
   rb_scan_args(argc, argv, "12", &path, &optarg, &size);

   rb_warning("(init) Setting up options arg");
   if(NIL_P(optarg)) {
      options = O_CREAT;
   } else {
      options = (int)NUM2INT(optarg);
   }

   rb_warning("(init) Setting up size arg");
   if(NIL_P(size)) {
      size = (size_t)INT2FIX(0);
   }

   rb_warning("(init) klass is type %X", TYPE(klass));
   Data_Get_Struct(klass, jlog_obj, jo);

   rb_warning("(init) Setting up jo: %X", TYPE(jo));
   jo->ctx = jlog_new(StringValuePtr(path));
   jo->path = strdup(StringValuePtr(path));
   jo->auto_checkpoint = 0;
   jo->start = zeroed;
   jo->prev = zeroed;
   jo->last = zeroed;
   jo->end = zeroed;


   rb_warning("(init) Testing object creation");
   if(!jo->ctx) {
      rJlog_free(jo);
      rb_raise(eJlog, "jlog_new(%s) failed", StringValuePtr(path));
   }

   rb_warning("(init) Handling jlog creation");
   if(options & O_CREAT) {
      rb_warning("(init) Initializing Jlog context");
      if(jlog_ctx_init(jo->ctx) != 0) {
         if(jlog_ctx_err(jo->ctx) == JLOG_ERR_CREATE_EXISTS) {
            if(options & O_EXCL) {
               rb_warning("(init) O_EXCL - Trying to free the object");
               rJlog_free(jo);
               rb_raise(eJlog, "file already exists: %s", StringValuePtr(path));
            }
         }else {
            rJlog_raise(jo, "Error initializing jlog");
         }
      }
      rb_warning("(init) Closing Jlog");
      jlog_ctx_close(jo->ctx);
      jo->ctx = jlog_new(StringValuePtr(path));
      if(!jo->ctx) {
         rJlog_free(jo);
         rb_raise(eJlog, "jlog_new(%s) failed after successful init", StringValuePtr(path));
      }
      rb_warning("(init) Populate Subscribers");
      rJlog_populate_subscribers(klass);
   } 

   if(!jo) { 
      rb_raise(eJlog, "jo wasn't initialized.");
   }

   return klass;
}

static VALUE rJlog_s_alloc(VALUE klass)
{
   Jlog jo = ALLOC(jlog_obj);

   return Data_Wrap_Struct(klass, rJlog_mark, rJlog_free, jo);
}

VALUE rJlog_add_subscriber(int argc, VALUE* argv, VALUE self)
{
   VALUE s;
   VALUE w;
   long whence;
   Jlog jo;

   rb_scan_args(argc, argv, "11", &s, &w);

   if(NIL_P(w)) {
      whence = 0;
   } else {
      whence = NUM2LONG(w);
   }

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx || jlog_ctx_add_subscriber(jo->ctx, StringValuePtr(s), whence) != 0) {
      return Qfalse;
   }

   rJlog_populate_subscribers(self);

   return Qtrue;
}


VALUE rJlog_remove_subscriber(VALUE self, VALUE subscriber)
{
   Jlog jo;

   Data_Get_Struct(self, jlog_obj, jo);
   int res = jlog_ctx_remove_subscriber(jo->ctx, StringValuePtr(subscriber));
   if(!jo || !jo->ctx || res != 0)
   {
      rb_warning(stderr, "\nResult of remove command is %d\n", res);
      return res;
   }

   rJlog_populate_subscribers(self);

   return Qtrue;
}

void rJlog_populate_subscribers(VALUE self)
{
   char **list;
   int i;
   Jlog jo;
   VALUE subscribers = rb_ary_new();

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx)
   {
      rb_raise(eJlog, "Invalid jlog context");
   }

   jlog_ctx_list_subscribers(jo->ctx, &list);
   rb_warning("(populate_subscribers) Looping over subscribers.");
   for(i=0; list[i]; i++ ) {
      rb_warning("(populate_subscribers) Pushing [%d] %s.", i, list[i]);
      rb_ary_push(subscribers, rb_str_new2(list[i]));
   }
   jlog_ctx_list_subscribers_dispose(jo->ctx, list);

   rb_iv_set(self, "@subscribers", subscribers);
}

VALUE rJlog_list_subscribers(VALUE self)
{
   Jlog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx)
   {
      rb_raise(eJlog, "Invalid jlog context");
   }

//   rJlog_populate_subscribers(self);

   return rb_iv_get(self, "@subscribers");
}

VALUE rJlog_raw_size(VALUE self)
{
   size_t size;
   Jlog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }
   size = jlog_raw_size(jo->ctx);

   return INT2NUM(size);
}

VALUE rJlog_close(VALUE self)
{
   Jlog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) return Qnil;

   jlog_ctx_close(jo->ctx);
   jo->ctx = NULL;

   return Qtrue;
}

VALUE rJlog_inspect(VALUE self)
{
   Jlog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) return Qnil;

   // XXX Fill in inspect call data 

   return Qtrue;
}

VALUE rJlog_destroy(VALUE self)
{
   Jlog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo) return Qnil;

   rJlog_free(jo);

   return Qtrue;
}

VALUE rJlog_W_open(VALUE self)
{
   Jlog_Writer jo;

   rb_warning("(W_open) Get and check context...");
   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

   rb_warning("(W_open) Opening jlog for write.");
   if(jlog_ctx_open_writer(jo->ctx) != 0) {
      rJlog_raise(jo, "jlog_ctx_open_writer failed");
   }

   return Qtrue;
}

VALUE rJlog_W_write(VALUE self, VALUE msg)
{
   int err;
   Jlog_Writer jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

#if !defined(RSTRING_LEN)
#  define RSTRING_LEN(x) (RSTRING(x)->len)
#endif
   rb_warning("1");
   err = jlog_ctx_write(jo->ctx, StringValuePtr(msg), (size_t) RSTRING_LEN(msg));
   rb_warning("2");
   if(err != 0) {
      rb_warning("(JW::write) write failed (%d) %s!", jlog_ctx_err(jo->ctx), jlog_ctx_err_string(jo->ctx));
      return Qfalse;
   } else {
      rb_warning("(JW::write) wrote %s!", (char *) StringValuePtr(msg));
      return Qtrue;
   }
}


VALUE rJlog_R_open(VALUE self, VALUE subscriber)
{
   Jlog_Reader jo;
   int err;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

   rb_warning("(JR::open) Opening %s", (char *) StringValuePtr(subscriber));
   err = jlog_ctx_open_reader(jo->ctx, StringValuePtr(subscriber));

   if(err != 0) {
      rb_warning("(JR::open) open failed (%d) %s!", jlog_ctx_err(jo->ctx), jlog_ctx_err_string(jo->ctx));
      rJlog_raise(jo, "jlog_ctx_open_reader failed");
   }

   return Qtrue;
}


VALUE rJlog_R_read(VALUE self)
{
   const jlog_id epoch = {0, 0};
   jlog_id cur = {0, 0};
   jlog_message message;
   int cnt;
   Jlog_Reader jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

   rb_warning("(JR::read) Check start");
   // If start is unset, read the interval
   if(jo->error || !memcmp(&jo->start, &epoch, sizeof(jlog_id)))
   {
      jo->error = 0;
      cnt = jlog_ctx_read_interval(jo->ctx, &jo->start, &jo->end);
      if(cnt == 0 || (cnt == -1 && jlog_ctx_err(jo->ctx) == JLOG_ERR_FILE_OPEN)) {
         jo->start = epoch;
         jo->end = epoch;
         return Qnil;
      }
      else if(cnt == -1)
         rJlog_raise(jo, "jlog_ctx_read_interval_failed");
   }

   rb_warning("(JR::read) Check last");
   // If last is unset, start at the beginning
   if(!memcmp(&jo->last, &epoch, sizeof(jlog_id))) {
      cur = jo->start;
   } else {
      // if we've already read the end, return; otherwise advance
      cur = jo->last;
      if(!memcmp(&jo->prev, &jo->end, sizeof(jlog_id))) {
         jo->start = epoch;
         jo->end = epoch;
         return Qnil;
      }
      jlog_ctx_advance_id(jo->ctx, &jo->last, &cur, &jo->end);
      if(!memcmp(&jo->last, &cur, sizeof(jlog_id))) {
            jo->start = epoch;
            jo->end = epoch;
            return Qnil;
      }
   }

   rb_warning("(JR::read) Read Message");
   if(jlog_ctx_read_message(jo->ctx, &cur, &message) != 0) {
      if(jlog_ctx_err(jo->ctx) == JLOG_ERR_FILE_OPEN) {
         jo->error = 1;
         rJlog_raise(jo, "jlog_ctx_read_message failed");
         return Qnil;
      }

      // read failed; raise error but recover if read is retried
      jo->error = 1;
      rJlog_raise(jo, "read failed");
   }

   rb_warning("(JR::read) Handle Autocheckpoint");
   if(jo->auto_checkpoint) {
      if(jlog_ctx_read_checkpoint(jo->ctx, &cur) != 0)
         rJlog_raise(jo, "checkpoint failed");

      // must reread the interval after a checkpoint
      jo->last = epoch;
      jo->prev = epoch;
      jo->start = epoch;
      jo->end = epoch;
   } else {
      // update last
      jo->prev = jo->last;
      jo->last = cur;
   }

   rb_warning("(JR::read) Returning: %p", message.mess);
   return rb_str_new2(message.mess);
}


VALUE rJlog_R_rewind(VALUE self)
{
   Jlog_Reader jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

   jo->last = jo->prev;

   return Qtrue;
}


VALUE rJlog_R_checkpoint(VALUE self)
{
   jlog_id epoch = { 0, 0 };
   Jlog_Reader jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

   if(memcmp(&jo->last, &epoch, sizeof(jlog_id)))
   {
      jlog_ctx_read_checkpoint(jo->ctx, &jo->last);

      // re-read the interval
      jo->last = epoch;
      jo->start = epoch;
      jo->end = epoch;
   }

   return Qtrue;
}


VALUE rJlog_R_auto_checkpoint(int argc, VALUE *argv, VALUE self)
{
   Jlog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

   if(argc > 0) {
      int ac = FIX2INT(argv[0]);
      jo->auto_checkpoint = ac;
   }

   return INT2FIX(jo->auto_checkpoint);
}


void Init_jlog(void) {
   cJlog = rb_define_class("Jlog", rb_cObject);
   cJlogWriter = rb_define_class_under(cJlog, "Writer", cJlog);
   cJlogReader = rb_define_class_under(cJlog, "Reader", cJlog);

   eJlog = rb_define_class_under(cJlog, "Error", rb_eStandardError);

   rb_define_method(cJlog, "initialize", rJlog_initialize, -1);
   rb_define_alloc_func(cJlog, rJlog_s_alloc);

   rb_define_method(cJlog, "add_subscriber", rJlog_add_subscriber, -1);
   rb_define_method(cJlog, "remove_subscriber", rJlog_remove_subscriber, 1);
   rb_define_method(cJlog, "list_subscribers", rJlog_list_subscribers, 0);
   rb_define_method(cJlog, "raw_size", rJlog_raw_size, 0);
   rb_define_method(cJlog, "close", rJlog_close, 0);
   rb_define_method(cJlog, "destroy", rJlog_destroy, 0);
   rb_define_method(cJlog, "inspect", rJlog_inspect, 0);

   rb_define_alias(cJlog, "size", "raw_size");

   rb_define_method(cJlogWriter, "initialize", rJlog_initialize, -1);
   rb_define_alloc_func(cJlogWriter, rJlog_s_alloc);

   rb_define_method(cJlogWriter, "open", rJlog_W_open, 0);
   rb_define_method(cJlogWriter, "write", rJlog_W_write, 1);

   rb_define_method(cJlogReader, "initialize", rJlog_initialize, -1);
   rb_define_alloc_func(cJlogReader, rJlog_s_alloc);

   rb_define_method(cJlogReader, "open", rJlog_R_open, 1);
   rb_define_method(cJlogReader, "read", rJlog_R_read, 0);
   rb_define_method(cJlogReader, "rewind", rJlog_R_rewind, 0);
   rb_define_method(cJlogReader, "checkpoint", rJlog_R_checkpoint, 0);
   rb_define_method(cJlogReader, "auto_checkpoint", rJlog_R_auto_checkpoint, -1);
}
