bin_PROGRAMS = misdn_info misdn_log misdn_E1test isdn_text2wireshark
sbin_PROGRAMS = misdn_rename misdn_cleanl2 misdnlogger

misdn_info_SOURCES = info.c
misdn_log_SOURCES = loghex.c
misdn_rename_SOURCES = rename.c
misdn_cleanl2_SOURCES = cleanl2.c
misdn_E1test_SOURCES = E1test.c
misdnlogger_SOURCES = logger.h logger.c logger_config_parser.l
misdnlogger_LDADD = ../lib/libmisdn.la
isdn_text2wireshark_SOURCES = text_wireshark.c

AM_CPPFLAGS = -Wall -Werror -I$(top_srcdir)/include $(_MEMLEAKDEBUG)

CLEANFILES = *~

EXTRA_DIST = misdnlogger.conf.sample

install-data-local:
	install -d $(DESTDIR)$(sysconfdir)
	install -m 644 misdnlogger.conf.sample $(DESTDIR)$(sysconfdir)/misdnlogger.conf

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v 'misdnlogger.conf'
