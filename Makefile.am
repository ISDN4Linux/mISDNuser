if OPT_EXAMPLE
    MAYBE_EXAMPLE = example
endif

SUBDIRS = include lib tools bridge l1oip $(MAYBE_EXAMPLE)

CLEANFILES = *~

ChangeLog: .git/objects
	git log > $@

