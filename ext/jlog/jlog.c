#include "ruby.h"
#include "jlog.h"
#include "fcntl.h"
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

typedef jlog_obj * JLog;
typedef jlog_obj * JLog_Writer;
typedef jlog_obj * JLog_Reader;

VALUE cJLog;
VALUE cJLogWriter;
VALUE cJLogReader;
VALUE eJLog;

void rJLog_populate_subscribers(VALUE);

void rJLog_mark(JLog jo) {
}

void rJLog_free(JLog jo) {
   if(jo->ctx) {
      jlog_ctx_close(jo->ctx);
   }

   rb_warning("(rJLog_free) Freeing path");
   if(jo->path) {
      xfree(jo->path);
   }

   if(jo) {
     rb_warning("(rJLog_free) Freeing jo");
     xfree(jo);
   }
}

void rJLog_raise(JLog jo, char* mess)
{
   VALUE e = rb_exc_new2(eJLog, mess);
   rb_iv_set(e, "error", INT2FIX(jlog_ctx_err(jo->ctx)));
   rb_iv_set(e, "errstr", rb_str_new2(jlog_ctx_err_string(jo->ctx)));
   rb_iv_set(e, "errno", INT2FIX(jlog_ctx_errno(jo->ctx)));

   rb_raise(eJLog, "%s: %d %s", mess, jlog_ctx_err(jo->ctx),
                               jlog_ctx_err_string(jo->ctx));

   //rb_exc_raise(e);
}

VALUE rJLog_initialize(int argc, VALUE* argv, VALUE klass) 
{
   int options;
   JLog jo;
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
      rJLog_free(jo);
      rb_raise(eJLog, "jlog_new(%s) failed", StringValuePtr(path));
   }

   rb_warning("(init) Handling jlog creation");
   if(options & O_CREAT) {
      rb_warning("(init) Initializing JLog context");
      if(jlog_ctx_init(jo->ctx) != 0) {
         if(jlog_ctx_err(jo->ctx) == JLOG_ERR_CREATE_EXISTS) {
            if(options & O_EXCL) {
               rb_warning("(init) O_EXCL - Trying to free the object");
               rJLog_free(jo);
               rb_raise(eJLog, "file already exists: %s", StringValuePtr(path));
            }
         }else {
            rJLog_raise(jo, "Error initializing jlog");
         }
      }
      rb_warning("(init) Closing JLog");
      jlog_ctx_close(jo->ctx);
      jo->ctx = jlog_new(StringValuePtr(path));
      if(!jo->ctx) {
         rJLog_free(jo);
         rb_raise(eJLog, "jlog_new(%s) failed after successful init", StringValuePtr(path));
      }
      rb_warning("(init) Populate Subscribers");
      rJLog_populate_subscribers(klass);
   } 

   if(!jo) { 
      rb_raise(eJLog, "jo wasn't initialized.");
   }

   return klass;
}

static VALUE rJLog_s_alloc(VALUE klass)
{
   JLog jo = ALLOC(jlog_obj);

   return Data_Wrap_Struct(klass, rJLog_mark, rJLog_free, jo);
}

VALUE rJLog_add_subscriber(int argc, VALUE* argv, VALUE self)
{
   VALUE s;
   VALUE w;
   long whence;
   JLog jo;

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

   rJLog_populate_subscribers(self);

   return Qtrue;
}


VALUE rJLog_remove_subscriber(VALUE self, VALUE subscriber)
{
   JLog jo;

   Data_Get_Struct(self, jlog_obj, jo);
   int res = jlog_ctx_remove_subscriber(jo->ctx, StringValuePtr(subscriber));
   if(!jo || !jo->ctx || res != 0)
   {
      rb_warning(stderr, "\nResult of remove command is %d\n", res);
      //rb_raise(eJLog, "FAILED");
      return res;
   }

   rJLog_populate_subscribers(self);

   return Qtrue;
}

void rJLog_populate_subscribers(VALUE self)
{
   char **list;
   int i;
   JLog jo;
   VALUE subscribers = rb_ary_new();

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx)
   {
      rb_raise(eJLog, "Invalid jlog context");
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

VALUE rJLog_list_subscribers(VALUE self)
{
   JLog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx)
   {
      rb_raise(eJLog, "Invalid jlog context");
   }

//   rJLog_populate_subscribers(self);

   return rb_iv_get(self, "@subscribers");
}

VALUE rJLog_raw_size(VALUE self)
{
   size_t size;
   JLog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
   }
   size = jlog_raw_size(jo->ctx);

   return INT2NUM(size);
}

VALUE rJLog_close(VALUE self)
{
   JLog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) return Qnil;

   jlog_ctx_close(jo->ctx);
   jo->ctx = NULL;

   return Qtrue;
}

VALUE rJLog_inspect(VALUE self)
{
   JLog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) return Qnil;

   // XXX Fill in inspect call data 

   return Qtrue;
}

VALUE rJLog_destroy(VALUE self)
{
   JLog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo) return Qnil;

   rJLog_free(jo);

   return Qtrue;
}

VALUE rJLog_W_open(VALUE self)
{
   JLog_Writer jo;

   rb_warning("(W_open) Get and check context...");
   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
   }

   rb_warning("(W_open) Opening jlog for write.");
   if(jlog_ctx_open_writer(jo->ctx) != 0) {
      rJLog_raise(jo, "jlog_ctx_open_writer failed");
   }

   return Qtrue;
}

//VALUE rJLog_W_write(int argc, VALUE *argv, VALUE self)
VALUE rJLog_W_write(VALUE self, VALUE msg)
{
//   VALUE msg;
//   VALUE ts;
   int err;
//   jlog_message m;
//   struct timeval t;
   JLog_Writer jo;

//   rb_scan_args(argc, argv, "10", &msg, &ts);

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
   }

#if !defined(RSTRING_LEN)
#  define RSTRING_LEN(x) (RSTRING(x)->len)
#endif
/*
   if(!NIL_P(ts)) {
      t.tv_sec = (time_t) NUM2INT(ts);
      t.tv_usec = 0;
   } else {
      gettimeofday(&t, NULL);
   }

   m.mess = StringValuePtr(msg);
   m.mess_len = RSTRING_LEN(msg);

   //XXX Implement write_message as an optional call if ts is not nil
   //if((err = jlog_ctx_write_message(jo->ctx, &m, ts ? &t : NULL)) < 0) {
*/
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


VALUE rJLog_R_open(VALUE self, VALUE subscriber)
{
   JLog_Reader jo;
   int err;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
   }

   rb_warning("(JR::open) Opening %s", (char *) StringValuePtr(subscriber));
   err = jlog_ctx_open_reader(jo->ctx, StringValuePtr(subscriber));

   if(err != 0) {
      rb_warning("(JR::open) open failed (%d) %s!", jlog_ctx_err(jo->ctx), jlog_ctx_err_string(jo->ctx));
      rJLog_raise(jo, "jlog_ctx_open_reader failed");
   }

   return Qtrue;
}


VALUE rJLog_R_read(VALUE self)
{
   const jlog_id epoch = {0, 0};
   jlog_id cur = {0, 0};
   jlog_message message;
   int cnt;
   JLog_Reader jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
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
         rJLog_raise(jo, "jlog_ctx_read_interval_failed");
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
         rJLog_raise(jo, "jlog_ctx_read_message failed");
         return Qnil;
      }

      // read failed; raise error but recover if read is retried
      jo->error = 1;
      rJLog_raise(jo, "read failed");
   }

   rb_warning("(JR::read) Handle Autocheckpoint");
   if(jo->auto_checkpoint) {
      if(jlog_ctx_read_checkpoint(jo->ctx, &cur) != 0)
         rJLog_raise(jo, "checkpoint failed");

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

   rb_warning("(JR::read) Returning: %s", message.mess);
   return rb_str_new2(message.mess);
}


VALUE rJLog_R_rewind(VALUE self)
{
   JLog_Reader jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
   }

   jo->last = jo->prev;

   return Qtrue;
}


VALUE rJLog_R_checkpoint(VALUE self)
{
   jlog_id epoch = { 0, 0 };
   JLog_Reader jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
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


VALUE rJLog_R_auto_checkpoint(int argc, VALUE *argv, VALUE self)
{
   JLog jo;

   Data_Get_Struct(self, jlog_obj, jo);

   if(!jo || !jo->ctx) {
      rb_raise(eJLog, "Invalid jlog context");
   }

   if(argc > 0) {
      int ac = FIX2INT(argv[0]);
      jo->auto_checkpoint = ac;
   }

   return INT2FIX(jo->auto_checkpoint);
}


void Init_jlog(void) {
   cJLog = rb_define_class("JLog", rb_cObject);
   cJLogWriter = rb_define_class_under(cJLog, "Writer", cJLog);
   cJLogReader = rb_define_class_under(cJLog, "Reader", cJLog);

   eJLog = rb_define_class_under(cJLog, "Error", rb_eStandardError);

//   rb_define_singleton_method(cJLog, "new", rJLog_new, -1);
   rb_define_method(cJLog, "initialize", rJLog_initialize, -1);
   rb_define_alloc_func(cJLog, rJLog_s_alloc);

   rb_define_method(cJLog, "add_subscriber", rJLog_add_subscriber, -1);
   rb_define_method(cJLog, "remove_subscriber", rJLog_remove_subscriber, 1);
   rb_define_method(cJLog, "list_subscribers", rJLog_list_subscribers, 0);
   rb_define_method(cJLog, "raw_size", rJLog_raw_size, 0);
   rb_define_method(cJLog, "close", rJLog_close, 0);

   rb_define_method(cJLog, "destroy", rJLog_destroy, 0);
   rb_define_method(cJLog, "inspect", rJLog_inspect, 0);

   rb_define_alias(cJLog, "size", "raw_size");

//   rb_define_singleton_method(cJLogWriter, "new", rJLog_new, -1);
   rb_define_method(cJLogWriter, "initialize", rJLog_initialize, -1);
   rb_define_alloc_func(cJLogWriter, rJLog_s_alloc);

   rb_define_method(cJLogWriter, "open", rJLog_W_open, 0);
   //rb_define_method(cJLogWriter, "write", rJLog_W_write, -1);
   rb_define_method(cJLogWriter, "write", rJLog_W_write, 1);

//   rb_define_singleton_method(cJLogReader, "new", rJLog_new, -1);
   rb_define_method(cJLogReader, "initialize", rJLog_initialize, -1);
   rb_define_alloc_func(cJLogReader, rJLog_s_alloc);

   rb_define_method(cJLogReader, "open", rJLog_R_open, 1);
   rb_define_method(cJLogReader, "read", rJLog_R_read, 0);
   rb_define_method(cJLogReader, "rewind", rJLog_R_rewind, 0);
   rb_define_method(cJLogReader, "checkpoint", rJLog_R_checkpoint, 0);
   rb_define_method(cJLogReader, "auto_checkpoint", rJLog_R_auto_checkpoint, -1);
}


/* ErrorType message, p 
 * message "; error: %d (%s) errno: %d (%s)",
 * jlog_ctx_err(p->ctx), jlog_ctx_err_string(p->ctx), jlog_ctx_errno(p->ctx), strerror(jlog_ctx_errno(j->ctx))
*/

