CC = gcc

CFLAGS += -DVERBOSE
CFLAGS += -DPORT=50501

all: mISDNdebugtool

install: mISDNdebugtool
	install -m 755 mISDNdebugtool $(INSTALL_PREFIX)/usr/bin

clean:
	rm -f mISDNdebugtool *.o

distclean: clean

