bin_PROGRAMS = misdn_info misdn_log misdn_E1test isdn_text2wireshark
sbin_PROGRAMS = misdn_rename misdn_cleanl2 misdn_call_log

misdn_info_SOURCES = info.c
misdn_log_SOURCES = loghex.c
misdn_rename_SOURCES = rename.c
misdn_cleanl2_SOURCES = cleanl2.c
misdn_E1test_SOURCES = E1test.c
misdn_call_log_SOURCES = call_log.h call_log.c
misdn_call_log_LDADD = ../lib/libmisdn.la
isdn_text2wireshark_SOURCES = text_wireshark.c

AM_CPPFLAGS = -Wall -Werror -I$(top_srcdir)/include $(_MEMLEAKDEBUG)

CLEANFILES = *~
