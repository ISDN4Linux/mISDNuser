all:
		@if test ! -f configure ; then \
			$(MAKE) configure ; \
		fi
		@if test ! -f Makefile.in ; then \
			$(MAKE) Makefile.in ; \
		fi
		@if test -f Makefile ; then \
			$(MAKE) -f Makefile $@; \
		else \
			echo "Please run ./configure"; \
		fi

%:
		@if test -f Makefile ; then \
			$(MAKE) -f Makefile $@; \
		else \
			echo "Please run ./configure"; \
		fi

aclocal.m4:
		aclocal

ltmain.sh:
		libtoolize --install

Makefile.in:	aclocal.m4
		-automake --add-missing
		autoconf

configure:	ltmain.sh Makefile.in aclocal.m4

cleanauto:
		-if test -f Makefile ; then \
			$(MAKE) -f Makefile distclean ; \
		fi
		-rm -rf m4 autom4te.cache 
		-rm install-sh missing aclocal.m4 ltmain.sh depcomp 
		-rm config.sub config.guess configure
		-find . -name Makefile.in -exec rm {} \;

.PHONY:		all configure
