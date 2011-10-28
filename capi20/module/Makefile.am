
lib_capi_mod_misdn_la_SOURCES = capi_mod_misdn.c
lib_capi_mod_misdn_la_CFLAGS = -fno-strict-aliasing -Wall
lib_capi_mod_misdn_la_LDFLAGS = -shared -version-info @CAPI_MODULE_LOADER_VERSION@:0:0

modulesdir = $(libdir)/capi
modules_LTLIBRARIES = lib_capi_mod_misdn.la

AM_CPPFLAGS = -I$(top_srcdir)/include -Wall -Werror

CLEANFILES = *~
