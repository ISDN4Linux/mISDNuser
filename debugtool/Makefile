CC = gcc

CFLAGS += -Wall $(EXTRA_CFLAGS)

all: mISDNdebugtool

install: mISDNdebugtool
	install -m 755 mISDNdebugtool $(INSTALL_PREFIX)/usr/bin

clean:
	rm -f mISDNdebugtool *.o

distclean: clean

