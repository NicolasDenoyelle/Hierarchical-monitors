AM_CFLAGS=-DCC=$(CC) -DSTATS_PLUGINS=@stat_plugins@

include_HEADERS=hmon.h hmon
lib_LTLIBRARIES=libhmon.la
libhmon_la_SOURCES=hmonitor.c harray.c synchronize.c hwloc_utils.c proc.c parser.c scanner.c console.c plugin.c sampling.c

bin_PROGRAMS=hmonitor
hmon_SOURCES=main.c
LDADD=-lhwloc
hmon_LDADD=libhmon.la

parser.c: parser.y 
	$(YACC) -o $@ --defines=parser.h $<

scanner.c: scanner.l
	$(LEX) -o $@ $^

clean-local:
	rm parser.c parser.h scanner.c

