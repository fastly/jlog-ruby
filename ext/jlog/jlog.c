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

static VALUE message_sym;
static VALUE timestamp_sym;

void rJlog_populate_subscribers(VALUE);

void rJlog_mark(Jlog jo) { }

void rJlog_free(Jlog jo) {
   if(jo->ctx) {
      jlog_ctx_close(jo->ctx);
   }

   if(jo->path) {
      xfree(jo->path);
   }

   if(jo) {
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

   rb_scan_args(argc, argv, "12", &path, &optarg, &size);

   if(NIL_P(optarg)) {
      options = O_CREAT;
   } else {
      options = (int)NUM2INT(optarg);
   }

   if(NIL_P(size)) {
      size = (size_t)INT2FIX(0);
   }

   Data_Get_Struct(klass, jlog_obj, jo);

   jo->ctx = jlog_new(StringValuePtr(path));
   jo->path = strdup(StringValuePtr(path));
   jo->auto_checkpoint = 0;
   jo->start = zeroed;
   jo->prev = zeroed;
   jo->last = zeroed;
   jo->end = zeroed;


   if(!jo->ctx) {
      rJlog_free(jo);
      rb_raise(eJlog, "jlog_new(%s) failed", StringValuePtr(path));
   }

   if(options & O_CREAT) {
      if(jlog_ctx_init(jo->ctx) != 0) {
         if(jlog_ctx_err(jo->ctx) == JLOG_ERR_CREATE_EXISTS) {
            if(options & O_EXCL) {
               rJlog_free(jo);
               rb_raise(eJlog, "file already exists: %s", StringValuePtr(path));
            }
         }else {
            rJlog_raise(jo, "Error initializing jlog");
         }
      }
      jlog_ctx_close(jo->ctx);
      jo->ctx = jlog_new(StringValuePtr(path));
      if(!jo->ctx) {
         rJlog_free(jo);
         rb_raise(eJlog, "jlog_new(%s) failed after successful init", StringValuePtr(path));
      }
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
   for(i=0; list[i]; i++ ) {
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

   rJlog_populate_subscribers(self);

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

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

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
   err = jlog_ctx_write(jo->ctx, StringValuePtr(msg), (size_t) RSTRING_LEN(msg));
   if(err != 0) {
      return Qfalse;
   } else {
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

   err = jlog_ctx_open_reader(jo->ctx, StringValuePtr(subscriber));

   if(err != 0) {
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

   return rb_str_new2(message.mess);
}

VALUE rJlog_R_read_message(VALUE self)
{
   const jlog_id epoch = {0, 0};
   jlog_id cur = {0, 0};
   jlog_message message;
   int cnt;
   double ts;
   Jlog_Reader jo;
   VALUE message_hash;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJlog, "Invalid jlog context");
   }

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

   ts = message.header->tv_sec+(message.header->tv_usec/1000000.0);

   message_hash = rb_hash_new();
   rb_hash_aset(message_hash, message_sym, rb_str_new2(message.mess));
   rb_hash_aset(message_hash, timestamp_sym, rb_float_new(ts));

   
   return message_hash;
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
   message_sym = ID2SYM(rb_intern("message"));
   timestamp_sym = ID2SYM(rb_intern("timestamp"));

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
   rb_define_method(cJlogReader, "read_message", rJlog_R_read_message, 0);
   rb_define_method(cJlogReader, "rewind", rJlog_R_rewind, 0);
   rb_define_method(cJlogReader, "checkpoint", rJlog_R_checkpoint, 0);
   rb_define_method(cJlogReader, "auto_checkpoint", rJlog_R_auto_checkpoint, -1);
}
