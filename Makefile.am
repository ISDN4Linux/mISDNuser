
ACLOCAL_AMFLAGS = -I m4

export _MEMLEAKDEBUG

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

if GIT_REPO

ChangeLog: .git/objects
	git log > $@

endif