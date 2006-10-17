SOMAJOR=0
SOMINOR=0.0
SOVERSION=$(SOMAJOR).$(SOMINOR)

#CFLAGS += -DASN1_DEBUG

SUPPSERVOBJ = asn1.o asn1_enc.o asn1_generic.o asn1_aoc.o asn1_basic_service.o asn1_comp.o asn1_diversion.o asn1_address.o fac.o

all: libsuppserv.a libsuppserv.so.$(SOVERSION)

main: $(SUPPSERVOBJ)

install:
	install -m 755 libsuppserv.so.$(SOVERSION) $(INSTALL_PREFIX)/$(LIBDIR)
	ln -sf libsuppserv.so.$(SOVERSION) $(INSTALL_PREFIX)/$(LIBDIR)/libsuppserv.so.$(SOMAJOR)
	ln -sf libsuppserv.so.$(SOVERSION) $(INSTALL_PREFIX)/$(LIBDIR)/libsuppserv.so
	install -m 644 libsuppserv.a $(INSTALL_PREFIX)/$(LIBDIR)
	cp *.h $(INSTALL_PREFIX)/usr/include/mISDNuser/

libsuppserv.so.$(SOVERSION): $(SUPPSERVOBJ)
	$(CC) -shared -Xlinker -x -Wl,-soname,libsuppserv.so.$(SOMAJOR) -o $@ $^

libsuppserv.a: $(SUPPSERVOBJ)
	ar cr $@ $^
	ar s $@

clean:
	rm -f *.o *~ DEADJOE
	rm -f libsuppserv.a libsuppserv.so

distclean: clean
	rm -f *.a
