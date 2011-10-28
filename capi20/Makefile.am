sbin_PROGRAMS = mISDNcapid

mISDNcapid_SOURCES = daemon.c application.c lplci.c mplci.c ncci.c m_capi.h m_capi_sock.h \
			mc_buffer.c mc_buffer.h SupplementaryService.h SupplementaryService.c listen.c

mISDNcapid_LDADD = ../lib/libmisdn.la -lcapi20
mISDNcapid_LDFLAGS = -shared

AM_CPPFLAGS = -I$(top_srcdir)/include -Werror -Wall

CLEANFILES = *~

MISDN_SOCKET_DIR = /var/run/mISDNcapid

install-exec-local:
	mkdir -p $(DESTDIR)$(MISDN_SOCKET_DIR)