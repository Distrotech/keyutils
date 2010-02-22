CFLAGS		:= -g -Wall -O2
INSTALL		:= install
DESTDIR		:=
MAJOR		:= 0
VERSION		:= $(MAJOR).1
NO_GLIBC_KEYERR	:= 0
NO_GLIBC_KEYSYS	:= 0
BUILDFOR	:= 
LIBDIR		:= /lib

LNS		:= ln -sf

ifeq ($(NO_GLIBC_KEYERR),1)
CFLAGS	+= -DNO_GLIBC_KEYERR
LIBLIBS	:= -ldl -lc
else
LIBLIBS	:=
endif

ifeq ($(NO_GLIBC_KEYSYS),1)
CFLAGS	+= -DNO_GLIBC_KEYSYS
endif

ifeq ($(BUILDFOR),32-bit)
CFLAGS	+= -m32
LIBDIR	:= /lib
else
ifeq ($(BUILDFOR),64-bit)
CFLAGS	+= -m64
LIBDIR	:= /lib64
endif
endif

all: libkeyutil.so keyctl request-key


libkeyutil.so: libkeyutil.so.$(MAJOR)
	ln -sf $< $@

libkeyutil.so.$(MAJOR): libkeyutil.so.$(VERSION)
	ln -sf $< $@

libkeyutil.so.$(VERSION): keyutil.c keyutil.h Makefile
	$(CC) $(CFLAGS) -fPIC $(LDFLAGS) -shared -Wl,-soname,libkeyutil.so.$(MAJOR) -o $@ keyutil.c $(LIBLIBS)


keyctl: keyctl.c keyutil.h Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -L. -lkeyutil -Wl,-rpath,$(LIB)

request-key: request-key.c keyutil.h Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -L. -lkeyutil -Wl,-rpath,$(LIB)


install: all
	$(INSTALL) -D libkeyutil.so.$(VERSION) $(DESTDIR)/$(LIBDIR)/libkeyutil.so.$(VERSION)
	$(LNS) libkeyutil.so.$(VERSION) $(DESTDIR)/$(LIBDIR)/libkeyutil.so.$(MAJOR)
	$(LNS) libkeyutil.so.$(MAJOR) $(DESTDIR)/$(LIBDIR)/libkeyutil.so
	$(INSTALL) -D keyctl $(DESTDIR)/bin/keyctl
	$(INSTALL) -D request-key $(DESTDIR)/sbin/request-key
	$(INSTALL) -D request-key.conf $(DESTDIR)/etc/request-key.conf
	$(INSTALL) -D request-key-debug.sh $(DESTDIR)/usr/share/keyutils/request-key-debug.sh
	$(INSTALL) -D keyctl.1 $(DESTDIR)/usr/share/man/man1/keyctl.1
	$(INSTALL) -D request-key.conf.5 $(DESTDIR)/usr/share/man/man5/request-key.conf.5
	$(INSTALL) -D request-key.8 $(DESTDIR)/usr/share/man/man8/request-key.8
	$(INSTALL) -D keyutil.h $(DESTDIR)/usr/include/keyutil.h

clean:
	$(RM) libkeyutil.so libkeyutil.so.$(MAJOR) libkeyutil.so.$(VERSION)
	$(RM) keyctl request-key
	$(RM) *~
