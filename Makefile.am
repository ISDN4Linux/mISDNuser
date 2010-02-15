if OPT_EXAMPLE
    MAYBE_EXAMPLE = example
endif

if OPT_GUI
    MAYBE_GUI = guitools
endif

SUBDIRS = include lib suppserv tools bridge l1oip $(MAYBE_EXAMPLE) $(MAYBE_GUI)

CLEANFILES = *~

if GIT_REPO

ChangeLog: .git/objects
	git log > $@

endif