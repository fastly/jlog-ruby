require 'mkmf'

cflags = " -g -I. -I.. -I../.."
ldflags = " -L. -L.. -L../.. -ljlog"
$CPPFLAGS += cflags
$LDFLAGS += ldflags

have_library('jlog')
#have_header('jlog.h')
create_makefile('jlog/jlog')
