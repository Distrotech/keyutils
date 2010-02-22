CFLAGS		:= -g -O2 -Wall
INSTALL		:= install
DESTDIR		:=
MAJOR		:= 1
MINOR		:= 0
RELEASE		:=
VERSION		:= $(MAJOR).$(MINOR)$(RELEASE)
NO_GLIBC_KEYERR	:= 0
NO_GLIBC_KEYSYS	:= 0
BUILDFOR	:= 
LIBDIR		:= /lib
USRLIBDIR	:= /usr/lib
ARLIB		:= libkeyutils.a
DEVELLIB	:= libkeyutils.so
SONAME		:= libkeyutils.so.$(MAJOR)
LIBNAME		:= libkeyutils-$(VERSION).so

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
CFLAGS		+= -m32
LIBDIR		:= /lib
USRLIBDIR	:= /usr/lib
else
ifeq ($(BUILDFOR),64-bit)
CFLAGS		+= -m64
LIBDIR		:= /lib64
USRLIBDIR	:= /usr/lib64
endif
endif

all: $(ARLIB) $(DEVELLIB) keyctl request-key


$(ARLIB): keyutils.o
	$(AR) rcs $@ $<

keyutils.o: keyutils.c keyutils.h Makefile
	$(CC) $(CFLAGS) -UNO_GLIBC_KEYERR -o $@ -c $<


$(DEVELLIB): $(SONAME)
	ln -sf $< $@

$(SONAME): $(LIBNAME)
	ln -sf $< $@

LIBVERS := -shared -Wl,-soname,$(SONAME) -Wl,--version-script,version.lds

$(LIBNAME): keyutils.os version.lds Makefile
	$(CC) $(CFLAGS) -fPIC $(LDFLAGS) $(LIBVERS) -o $@ keyutils.os $(LIBLIBS)

keyutils.os: keyutils.c keyutils.h Makefile
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<


keyctl: keyctl.c keyutils.h Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -L. -lkeyutils -Wl,-rpath,$(LIB)

request-key: request-key.c keyutils.h Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -L. -lkeyutils -Wl,-rpath,$(LIB)


install: all
	$(INSTALL) -D $(ARLIB) $(DESTDIR)/$(USRLIBDIR)/$(ARLIB)
	$(INSTALL) -D $(LIBNAME) $(DESTDIR)/$(LIBDIR)/$(LIBNAME)
	$(LNS) $(LIBNAME) $(DESTDIR)/$(LIBDIR)/$(SONAME)
	mkdir -p $(DESTDIR)/$(USRLIBDIR)
	$(LNS) $(LIBDIR)/$(SONAME) $(DESTDIR)/$(USRLIBDIR)/$(DEVELLIB)
	$(INSTALL) -D keyctl $(DESTDIR)/bin/keyctl
	$(INSTALL) -D request-key $(DESTDIR)/sbin/request-key
	$(INSTALL) -D request-key.conf $(DESTDIR)/etc/request-key.conf
	$(INSTALL) -D request-key-debug.sh $(DESTDIR)/usr/share/keyutils/request-key-debug.sh
	$(INSTALL) -D keyctl.1 $(DESTDIR)/usr/share/man/man1/keyctl.1
	$(INSTALL) -D request-key.conf.5 $(DESTDIR)/usr/share/man/man5/request-key.conf.5
	$(INSTALL) -D request-key.8 $(DESTDIR)/usr/share/man/man8/request-key.8
	$(INSTALL) -D keyutils.h $(DESTDIR)/usr/include/keyutils.h

clean:
	$(RM) libkeyutils*
	$(RM) keyctl request-key
	$(RM) *.o *.os *~
	$(RM) debugfiles.list debugsources.list
