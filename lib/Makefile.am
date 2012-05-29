export _MEMLEAKDEBUG

ASN1_SRC =	asn1/asn1.c asn1/asn1_enc.c asn1/asn1_generic.c

SUPPSERV_SRC =	suppserv/aoc.c suppserv/basic_service.c suppserv/component.c \
		suppserv/diversion.c suppserv/address.c suppserv/ccbs.c \
		suppserv/fac.c suppserv/ect.c

SUPPSERV_INC = include/ccbs.h include/diversion.h include/ect.h include/asn1.h


MISC_SRC =	misc/debug.c misc/fsm.c misc/mbuffer.c misc/mtimer.c

LAYER3_SRC =	layer3/dss1net.c layer3/dss1user.c layer3/layer3.c layer3/mlayer3.c layer3/q931.c

INCLUDE_SRC =	include/debug.h include/dss1.h include/fsm.h include/helper.h include/layer3.h \
		include/mlist.h include/mtimer.h $(SUPPSERV_INC)

lib_LTLIBRARIES = libmisdn.la
libmisdn_la_SOURCES = $(MISC_SRC) $(LAYER3_SRC) $(INCLUDE_SRC) $(ASN1_SRC) $(SUPPSERV_SRC)
libmisdn_la_LDFLAGS = -version-info 1:0:0

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/include $(_MEMLEAKDEBUG)

CLEANFILES = *~ */*~
