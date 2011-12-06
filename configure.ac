#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([mISDNuser], [2.0.5], [i4ldeveloper@isdn4linux.de])
AC_CONFIG_SRCDIR([tools/])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h malloc.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

LT_INIT

AC_ARG_ENABLE([example],
	[  --enable-example   Include example programs],
	[case "${enableval}" in
		yes) example=true ;;
		no)  example=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-example]) ;;
	  esac],
	  [example=false])
                         	
AM_CONDITIONAL([OPT_EXAMPLE], [test "x$example" = xtrue])

AC_ARG_ENABLE([gui],
	[  --enable-gui   Include GUI programs],
	[case "${enableval}" in
		yes) gui=true ;;
		no)  gui=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-example]) ;;
	  esac],
	  [gui=false])
                         	
AM_CONDITIONAL([OPT_GUI], [test "x$gui" = xtrue])

if $gui
then
	AC_CHECK_LIB(QtCore, [qt_addObject],
		AC_MSG_RESULT(found),
		AC_MSG_ERROR([Qt library not found])
	)
	AC_ARG_VAR(QMAKE, [QT make helper])
	AC_CHECK_TOOL(QMAKE, qmake,[NotFound],)
	if test x$QMAKE = xNotFound
	then
		AC_MSG_ERROR([qmake not found])
		
	fi
fi

AC_ARG_ENABLE([memleak_debug],
	[  --enable-memleak_debug    Turn on debugging of memleaks],
	[case "${enableval}" in
		yes) memleak_debug=true ;;
		no)  memleak_debug=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-memleak_debug]) ;;
	esac],
	[debug=false])

AM_CONDITIONAL([MEMLEAKDEBUG], [test x$memleak_debug = xtrue])
if test x$memleak_debug = xtrue ; then
	MEMLEAKDEBUG_COMPILE=1
else
	MEMLEAKDEBUG_COMPILE=0
fi
AC_SUBST(MEMLEAKDEBUG_COMPILE)

AC_ARG_ENABLE([capi],
	[  --enable-capi   Include CAPI2.0 support],
	[case "${enableval}" in
		yes) capi=true ;;
		no)  capi=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-capi]) ;;
	  esac],
	  [capi=false])
                         	
AM_CONDITIONAL([OPT_CAPI], [test "x$capi" = xtrue])

AC_ARG_ENABLE([softdsp],
	[  --enable-softdsp   Include CAPI2.0 softdsp support for analog FAX],
	[case "${enableval}" in
		yes) softdsp=true ;;
		no)  softdsp=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-softdsp]) ;;
	  esac],
	  [softdsp=false])
                         	
AM_CONDITIONAL([OPT_SOFTDSP], [test "x$softdsp" = xtrue])

if $capi
then
	AC_CHECK_LIB(capi20, [capi20_isinstalled],,
		AC_MSG_ERROR([capi library not found])
	)
	AC_CHECK_HEADER([capi20.h],
		AC_CHECK_HEADER([capi_mod.h],,
			AC_MSG_ERROR([capi_mod.h do not exist - update capi4linux]),
		),
		AC_MSG_ERROR([capi20.h do not exist - install capi4linux development files]),
	)
	AC_COMPUTE_INT([CAPI_MODULE_LOADER_VERSION], [[MODULE_LOADER_VERSION]],
		[
		#include <sys/types.h>
		#include <capi_mod.h>
		],
		[AC_MSG_ERROR([capi library MODULE_LOADER_VERSION cannot calculated - update capi4linux])]
	)
	AC_MSG_RESULT([Capi library module loader version ${CAPI_MODULE_LOADER_VERSION} detected]) 
	AC_SUBST(CAPI_MODULE_LOADER_VERSION)
	if $softdsp
	then
		AC_CHECK_HEADER([spandsp.h],,
			AC_MSG_ERROR([spandsp header file not found - install spandsp development files])
		)
	fi
fi

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyname gettimeofday inet_ntoa memset select socket strcasecmp strchr strdup strerror strstr strtol])

# special checks
MISDN_CHECK_AF_ISDN


if test -d .git/objects
then
	git_repo=true
else
	git_repo=false
fi

AC_MISDN_GROUP

AC_SUBST(MISDN_GROUP)

AM_CONDITIONAL([GIT_REPO], [test "x$git_repo" = xtrue])
	
AC_CONFIG_FILES([Makefile
                 45-misdn.rules
                 include/Makefile
                 include/mISDN/mISDNcompat.h
                 lib/Makefile
                 bridge/Makefile
                 l1oip/Makefile
                 tools/Makefile
                 example/Makefile
                 guitools/Makefile
                 guitools/qmisdnwatch/Makefile
                 capi20/Makefile
                 capi20/module/Makefile
               ])

AC_OUTPUT
