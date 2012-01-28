bin_PROGRAMS = misdntestcon misdntestlayer1 misdntestlayer3 misdnportinfo
misdntestcon_SOURCES = testcon.c
misdntestcon_LDADD = ../lib/libmisdn.la
misdntestlayer1_SOURCES = testlayer1.c
misdntestlayer3_SOURCES = testlayer3.c
misdnportinfo_SOURCES = misdnportinfo.c
misdntestlayer3_LDADD = ../lib/libmisdn.la

AM_CPPFLAGS = -I$(top_srcdir)/include $(_MEMLEAKDEBUG)

CLEANFILES = *~
