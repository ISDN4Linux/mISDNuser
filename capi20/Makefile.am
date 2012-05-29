export _USE_SOFTFAX

sbin_PROGRAMS = mISDNcapid
mISDNcapid_SOURCES = daemon.c application.c lplci.c mplci.c ncci.c m_capi.h m_capi_sock.h \
			mc_buffer.c mc_buffer.h SupplementaryService.h SupplementaryService.c listen.c faxl3.c alaw.c alaw.h \
			sff.h sff.c g3_mh.h g3_mh.c
mISDNcapid_LDADD = ../lib/libmisdn.la -lcapi20
mISDNcapid_LDFLAGS = -shared

if OPT_SOFTDSP
	_USE_SOFTFAX = "-DUSE_SOFTFAX"
	mISDNcapid_LDADD += -lspandsp -ltiff
else
	_USE_SOFTFAX =
endif

AM_CPPFLAGS = -I$(top_srcdir)/include -Werror -Wall $(_USE_SOFTFAX) $(_MEMLEAKDEBUG) -DMISDN_GROUP=\"$(MISDN_GROUP)\"

EXTRA_DIST = capi20.conf.sample

CLEANFILES = *~

MISDN_SOCKET_DIR = $(localstatedir)/run/mISDNcapid

install-exec-hook:
	install  -d $(DESTDIR)$(MISDN_SOCKET_DIR)

install-data-local:
	install -d $(DESTDIR)$(sysconfdir)
	install -m 644 capi20.conf.sample $(DESTDIR)$(sysconfdir)/capi20.conf

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v 'capi20.conf'
