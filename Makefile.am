ACLOCAL_AMFLAGS=-I m4
AUTOMAKE_OPTIONS=foreign
SUBDIRS=src src/plugins/stat_default src/plugins/fake src/plugins/accumulate src/plugins/hierarchical src/plugins/system

if BUILD_PAPI
SUBDIRS+=src/plugins/papi
endif

if BUILD_MAQAO
SUBDIRS+=src/plugins/maqao
endif

if BUILD_LEARNING
SUBDIRS+=src/plugins/learning
endif

