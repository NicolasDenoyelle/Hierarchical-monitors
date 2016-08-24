AM_CFLAGS=-DCC=$(CC) -DSTATS_PLUGINS=@stat_plugins@

include_HEADERS=hmon.h hmon_utils.h
lib_LTLIBRARIES=libhmon.la
libhmon_la_SOURCES=hmon.c hmon_array.c hwloc.c proc.c parser.c scanner.c console.c plugin.c

bin_PROGRAMS=hmon
hmon_SOURCES=main.c
hmon_LDADD=libhmon.la

parser.c: parser.y 
	$(YACC) -o $@ --defines=parser.h $<

scanner.c: scanner.l
	$(LEX) -o $@ $^

clean-local:
	rm parser.c parser.h scanner.c

