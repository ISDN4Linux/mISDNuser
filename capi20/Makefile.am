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

AM_CPPFLAGS = -I$(top_srcdir)/include -Werror -Wall $(_USE_SOFTFAX)

CLEANFILES = *~

MISDN_SOCKET_DIR = /var/run/mISDNcapid
MISDN_CAPI_GROUP = dialout

install-exec-hook:
	mkdir -p $(DESTDIR)$(MISDN_SOCKET_DIR)
	chmod 775 $(DESTDIR)$(MISDN_SOCKET_DIR)
	chgrp $(MISDN_CAPI_GROUP) $(DESTDIR)$(MISDN_SOCKET_DIR)
	chgrp $(MISDN_CAPI_GROUP) $(DESTDIR)$(sbindir)/mISDNcapid
	chmod 2755 $(DESTDIR)$(sbindir)/mISDNcapid

