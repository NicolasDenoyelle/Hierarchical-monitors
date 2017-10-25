AM_CFLAGS=-DCC=$(CC) -I$(abs_top_builddir)/hmon -I$(abs_top_builddir)

lib_LTLIBRARIES=libhmon.la
libhmon_la_SOURCES=hmonitor.c harray.c synchronize.c hwloc_utils.c proc.c parser.c scanner.c plugin.c sampling.c
include_HEADERS=hmon.h
hmonincludedir=$(includedir)/hmon
hmoninclude_HEADERS=hmon/harray.h hmon/hmonitor.h

bin_PROGRAMS=hmonitor
hmonitor_SOURCES=main.c
hmonitor_LDFLAGS=-lhwloc
hmonitor_LDADD=libhmon.la

if HMON_HAVE_LSTOPO
hmonitor_SOURCES+=lstopo.c
hmonitor_LDFLAGS+=-llstopo
else
hmonitor_SOURCES+=console.c
endif

parser.c: parser.y 
	$(YACC) -o $@ --defines=parser.h $<

scanner.c: scanner.l
	$(LEX) -o $@ $^

clean-local:
	rm -f parser.c parser.h scanner.c


