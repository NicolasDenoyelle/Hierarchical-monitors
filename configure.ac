#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([hierarchical-monitors], [1.0], [denoyelle.nicolas@gmail.com], [dlopen])
AM_INIT_AUTOMAKE([-Wall -Werror gnu])
AC_CONFIG_SRCDIR([./Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_FILES([src/Makefile])

# Checks for programs.
AC_PROG_YACC
AC_PROG_CC
AC_PROG_LEX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_AR

LT_INIT()

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([\
	clock_gettime\
	memmove\
	memset\
	select\
	strdup\
	strerror\
	snprintf\
	sscanf\
	unlink\
	va_list\
	bsearch\
	qsort\
	pthread_test_cancel\
	system])

# Checks for linux header files.
AC_CHECK_HEADERS([\
	float.h\
	inttypes.h\
	stdint.h\
	stdlib.h\
	stdio.h\
	string.h\
	sys/stat.h\
	sys/types.h\
	sys/wait.h\
	sys/timerfd.h\
	time.h\
	signal.h\
	pthread.h\
	dlfcn.h\
	dirent.h\
	stdarg.h])

# Check for linux libraries
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([m], [sqrt])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([rt], [clock_gettime])

# Check for hwloc.
AC_DEFUN([HWLOC_VERSION_CHECK],
	 [AC_RUN_IFELSE([AC_LANG_PROGRAM(
			[],
			[[#if HWLOC_API_VERSION >= 0x0020000
			  if(hwloc_get_api_version() < 0x20000) return 1;
			  #endif
			  return 0;
			]])],
		        [],
		        [AC_MSG_ERROR([You need an hwloc version with api version 0x20000 or later])])
	])
	 
AC_CHECK_HEADER([hwloc.h])
AC_CHECK_LIB(
	[hwloc],
	[hwloc_topology_init],
	[HWLOC_VERSION_CHECK],
	[AC_MSG_ERROR([hwloc library is required])])


# Check for statistic plugins to build
stat_plugins="defstats"

## Checks for learning plugin.
build_learning=yes
AC_CHECK_HEADERS([\
	gsl/gsl_blas.h \
        gsl/gsl_matrix_double.h\
	gsl/gsl_multimin.h\
	gsl/gsl_multifit.h\
	gsl/gsl_statistics.h\
	gsl/gsl_vector.h\
	gsl/gsl_matrix_double.h], [], [build_learning=no])

AC_CHECK_LIB([m],[cos], [], [build_learning=no])
AC_CHECK_LIB([gslcblas],[cblas_dgemm], [], [build_learning=no])
AC_CHECK_LIB([gsl],[gsl_blas_dgemm], [], [build_learning=no])

AS_IF([test "x$build_learning" = xyes], [stat_plugins="$stat_plugins learning"])

# Check for performance plugins
AC_CONFIG_FILES([src/plugins/stat_default/Makefile src/plugins/fake/Makefile src/plugins/accumulate/Makefile src/plugins/hierarchical/Makefile])

## Check for papi plugin
build_papi="yes"
AC_CHECK_LIB([papi], [PAPI_library_init], [], [build_papi=no])
AS_IF([test "x$build_papi" = xyes],
      [AS_IF([test `cat /proc/sys/kernel/perf_event_paranoid` -gt -1],
             [AC_MSG_NOTICE([If you are willing to use papi, you should set /proc/sys/kernel/perf_event_paranoid to -1])]
	    )
      ])

## Check for maqao plugin
build_maqao="yes"
AC_CHECK_LIB([lprof], [counting_start_counters], [], [build_maqao=no])
AS_IF([test "x$build_maqao" = xyes],
      [AS_IF([test `cat /proc/sys/kernel/perf_event_paranoid` -gt -1],
             [AC_MSG_NOTICE([If you are willing to use maqao, you should set /proc/sys/kernel/perf_event_paranoid to -1])]
	    )
      ])


AC_DEFINE_UNQUOTED([STAT_PLUGINS], ["$stat_plugins"], [desc])
m4_pattern_allow([STAT_PLUGINS])

AM_CONDITIONAL([BUILD_PAPI],     [test "x$build_papi" = xyes])
AM_COND_IF([BUILD_PAPI], [AC_CONFIG_FILES([src/plugins/papi/Makefile])])
AM_CONDITIONAL([BUILD_MAQAO],    [test "x$build_maqao" = xyes])
AM_COND_IF([BUILD_MAQAO], [AC_CONFIG_FILES([src/plugins/maqao/Makefile])])
AM_CONDITIONAL([BUILD_LEARNING], [test "x$build_learning" = xyes])
AM_COND_IF([BUILD_LEARNING], [AC_CONFIG_FILES([src/plugins/learning/Makefile])])

AC_OUTPUT(Makefile)

library_ok="yes"

echo "################################################################################################################"
echo "#                                               Configure summary                                              #"
echo "################################################################################################################"
printf "%-20s: %-3s\n" "hmon library" "$library_ok"
printf "%-20s: %-3s\n" "papi plugin" "$build_papi"
printf "%-20s: %-3s\n" "maqao plugin" "$build_maqao"
printf "%-20s: %-3s\n" "learning plugin" "$build_learning"

