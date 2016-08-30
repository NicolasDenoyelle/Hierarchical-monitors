AM_CFLAGS=-DCC=$(CC) -DSTATS_PLUGINS=@stat_plugins@

lib_LTLIBRARIES=libhmon.la
libhmon_la_SOURCES=hmonitor.c harray.c synchronize.c hwloc_utils.c proc.c parser.c scanner.c console.c plugin.c sampling.c
include_HEADERS=hmon.h
hmonincludedir=$(includedir)/hmon
hmoninclude_HEADERS=hmon/harray.h hmon/hmonitor.h

bin_PROGRAMS=hmonitor
hmonitor_SOURCES=main.c
LDADD=-lhwloc
hmonitor_LDADD=libhmon.la

parser.c: parser.y 
	$(YACC) -o $@ --defines=parser.h $<

scanner.c: scanner.l
	$(LEX) -o $@ $^

clean-local:
	rm -f parser.c parser.h scanner.c


