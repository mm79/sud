AM_YFLAGS = -d

sbin_PROGRAMS = sud
sud_SOURCES = conf.c conf_parser.y conf_lexer.l interactive.c noninteractive.c control.c main.c popen.c request.c session.c service.c utility.c extern.h conf.h control.h queue.h session.h
sud_CFLAGS = -Wall -DYY_NO_UNPUT

#INCLUDES = -I..
CLEANFILES = conf_lexer.c conf_parser.h conf_parser.c

install-data-hook:
	chmod 500 $(sbindir)/sud
