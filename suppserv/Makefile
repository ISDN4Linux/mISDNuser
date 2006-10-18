CC = gcc
AR = ar
RANLIB = ranlib

#CFLAGS += -DASN1_DEBUG

SUPPSERV_OBJ = asn1.o asn1_enc.o asn1_generic.o asn1_aoc.o asn1_basic_service.o asn1_comp.o asn1_diversion.o asn1_address.o fac.o
SUPPSERV_PICOBJ = $(SUPPSERV_OBJ:%.o=%.lo)

all: libsuppserv.a libsuppserv_pic.a libsuppserv.so

main: $(SUPPSERVOBJ)

install:
	install -m 644 libsuppserv.a $(INSTALL_PREFIX)/usr/lib
	install -m 644 libsuppserv_pic.a $(INSTALL_PREFIX)/usr/lib
	install -m 644 libsuppserv.so $(INSTALL_PREFIX)/usr/lib
	cp *.h $(INSTALL_PREFIX)/usr/include/mISDNuser/

libsuppserv_pic.a: $(SUPPSERV_PICOBJ)
	$(AR) cru $@ $<
	$(RANLIB) $@

libsuppserv.a: $(SUPPSERV_OBJ)
	$(AR) cru $@ $<
	$(RANLIB) $@

libsuppserv.so: $(SUPPSERV_OBJ)
	$(CC) -shared -Xlinker -x -o $@ $^


.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.c.lo:
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

clean:
	rm -f *.o *.lo *~ DEADJOE *.so
	rm -f libsuppserv.a libsuppserv_pic.a

distclean: clean
	rm -f *.a

.SUFFIXES: .lo
