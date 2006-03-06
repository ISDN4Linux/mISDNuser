# ifndef SF_DIR
# SF_DIR = /home/kkeil/speak_freely-7.2
# endif

mISDNLIB = $(mISDN_DIR)/lib/libmISDN.a
ISDNNETLIB = $(mISDN_DIR)/i4lnet/libisdnnet.a

HLIBINCL = $(mISDN_DIR)/include/mISDNlib.h
INETINCL = $(mISDN_DIR)/include/isdn_net.h

EXTRA_CFLAGS :=
EXTRA_INCLUDE :=
EXTRA_LIB :=

#GSM_DIR := ../../gsm-1.0-pl6

ifdef GSM_DIR
EXTRA_CFLAGS += -DGSM_COMPRESSION
EXTRA_INCLUDE += -I$(GSM_DIR)/inc
EXTRA_LIB += $(GSM_DIR)/lib/libgsm.a
endif

PROGRAMMS = voipisdn

all: $(PROGRAMMS)

install:
	for i in $(PROGRAMMS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done

INTERNET_PORT = 2074

CARGS	= -DInternet_Port=$(INTERNET_PORT)

CCFLAGS	= -O3 -DLINUX -DM_LITTLE_ENDIAN

LFLAGS	= -lncurses -lm -lrt -lpthread

DEBUG	= -g -DHEXDUMP

CFLAGS	:= $(CFLAGS) $(DEBUG) $(EXTRA_INCLUDE) \
	$(CARGS) $(DUPLEX) $(CCFLAGS) $(DOMAIN) $(EXTRA_CFLAGS)

LEX	= flex -8

VOIPISDNOBJ = voip_isdn.o rtpacket.o voip_timer.o \
		read_cfg.o voip_appl.o voip_isdn_app.o

voipisdn: $(VOIPISDNOBJ) $(ISDNNETLIB) $(mISDNLIB) $(EXTRA_LIB) \
		$(HLIBINCL) $(INETINCL) \
		globals.h iapplication.h
	$(CC) $(VOIPISDNOBJ) $(ISDNNETLIB) $(mISDNLIB) $(EXTRA_LIB) \
		$(LFLAGS) -o $@

rtpacket.o: rtpacket.c rtpacket.h \
		$(mISDN_DIR)/include/g711.h

voip_timer.o: voip_timer.c vitimer.h

voip_appl.o: voip_appl.c $(mISDN_DIR)/include/g711.h \
		globals.h rtpacket.h iapplication.h \
		$(HLIBINCL) $(INETINCL)

voip_isdn_app.o: voip_isdn_app.c \
		globals.h rtpacket.h iapplication.h \
		$(HLIBINCL) $(INETINCL)

voip_isdn.o: voip_isdn.c $(mISDN_DIR)/include/g711.h \
		globals.h rtpacket.h iapplication.h \
		$(HLIBINCL) $(INETINCL) 

cfg_lex.c: cfg.lex
	$(LEX) cfg.lex
	mv lex.yy.c cfg_lex.c

read_cfg.o: read_cfg.c cfg_lex.c cfg.lex iapplication.h

tstparse.o: tstparse.c

tstparse: tstparse.o read_cfg.o

clean:
	rm -f *.o cfg_lex.c DEADJOE
	find ./ -name '*~' -exec rm {} \;
	rm -f voipisdn

distclean: clean
	rm -f *.a $(PROGRAMMS) tstparse
