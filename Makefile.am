if OPT_EXAMPLE
    MAYBE_EXAMPLE = example
endif

SUBDIRS = include lib tools bridge $(MAYBE_EXAMPLE)

CLEANFILES = *~

ChangeLog: .git/objects
	git log > $@

