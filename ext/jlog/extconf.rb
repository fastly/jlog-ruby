require 'mkmf'

have_library('jlog')
have_header('jlog.h')
create_makefile('jlog/jlog')
