
ACLOCAL_AMFLAGS = -I m4

export _MEMLEAKDEBUG MISDN_CAPI_GROUP

MISDN_CAPI_GROUP=@MISDN_GROUP@

if MEMLEAKDEBUG
	_MEMLEAKDEBUG = "-DMEMLEAK_DEBUG=1"
else
	_MEMLEAKDEBUG =
endif

if OPT_EXAMPLE
    MAYBE_EXAMPLE = example
endif

if OPT_GUI
    MAYBE_GUI = guitools
endif

if OPT_CAPI
    MAYBE_CAPI = capi20 capi20/module
endif

SUBDIRS = include lib tools bridge l1oip $(MAYBE_EXAMPLE) $(MAYBE_GUI) $(MAYBE_CAPI)

CLEANFILES = *~
DISTCLEANFILES = 45-misdn.rules

EXTRA_DIST = m4

if GIT_REPO

ChangeLog: $(srcdir)/.git/objects
	git log > $@

endif

install-data-local:
	install -d $(DESTDIR)$(sysconfdir)/udev/rules.d
	install -m 644 45-misdn.rules $(DESTDIR)$(sysconfdir)/udev/rules.d/45-misdn.rules

distuninstallcheck_listfiles = \
	find . -type f -print | grep -v '45-misdn.rules'

