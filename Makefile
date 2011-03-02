CFLAGS		:= -g -O2 -Wall
INSTALL		:= install
DESTDIR		:=
MAJOR		:= 1
MINOR		:= 3
VERSION		:= $(MAJOR).$(MINOR)
NO_GLIBC_KEYERR	:= 0
NO_ARLIB	:= 0
ETCDIR		:= /etc
BINDIR		:= /bin
SBINDIR		:= /sbin
SHAREDIR	:= /usr/share/keyutils
MAN1		:= /usr/share/man/man1
MAN3		:= /usr/share/man/man3
MAN5		:= /usr/share/man/man5
MAN8		:= /usr/share/man/man8
INCLUDEDIR	:= /usr/include
ARLIB		:= libkeyutils.a
DEVELLIB	:= libkeyutils.so
SONAME		:= libkeyutils.so.$(MAJOR)
LIBNAME		:= libkeyutils.so.$(VERSION)

LIBDIR		:= $(shell ldd /usr/bin/make | grep '\(/libc\)' | sed -e 's!.*\(/.*\)/libc[.].*!\1!')
USRLIBDIR	:= $(patsubst /lib/%,/usr/lib/%,$(LIBDIR))
BUILDFOR	:= $(shell file /usr/bin/make | sed -e 's!.*ELF \(32\|64\)-bit.*!\1!')-bit

LNS		:= ln -sf

ifeq ($(NO_GLIBC_KEYERR),1)
CFLAGS	+= -DNO_GLIBC_KEYERR
LIBLIBS	:= -ldl -lc
else
LIBLIBS	:=
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

RPATH = -Wl,-rpath,$(LIBDIR)

ifeq ($(NO_ARLIB),0)
all: $(ARLIB)
$(ARLIB): keyutils.o
	$(AR) rcs $@ $<
endif

all: $(DEVELLIB) keyctl request-key

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


keyctl: keyctl.c keyutils.h Makefile -lkeyutils
	$(CC) -L. $(CFLAGS) $(LDFLAGS) $(RPATH) -o $@ $< -lkeyutils

request-key: request-key.c keyutils.h Makefile -lkeyutils
	$(CC) -L. $(CFLAGS) $(LDFLAGS) $(RPATH) -o $@ $< -lkeyutils


install: all
ifeq ($(NO_ARLIB),0)
	$(INSTALL) -D -m 0644 $(ARLIB) $(DESTDIR)$(USRLIBDIR)/$(ARLIB)
endif
	$(INSTALL) -D $(LIBNAME) $(DESTDIR)$(LIBDIR)/$(LIBNAME)
	$(LNS) $(LIBNAME) $(DESTDIR)$(LIBDIR)/$(SONAME)
	mkdir -p $(DESTDIR)$(USRLIBDIR)
	$(LNS) $(LIBDIR)/$(SONAME) $(DESTDIR)$(USRLIBDIR)/$(DEVELLIB)
	$(INSTALL) -D keyctl $(DESTDIR)$(BINDIR)/keyctl
	$(INSTALL) -D request-key $(DESTDIR)$(SBINDIR)/request-key
	$(INSTALL) -D request-key-debug.sh $(DESTDIR)$(SHAREDIR)/request-key-debug.sh
	$(INSTALL) -D -m 0644 request-key.conf $(DESTDIR)$(ETCDIR)/request-key.conf
	$(INSTALL) -D -m 0644 keyctl.1 $(DESTDIR)$(MAN1)/keyctl.1
	$(INSTALL) -D -m 0644 keyctl_chown.3 $(DESTDIR)$(MAN3)/keyctl_chown.3
	$(INSTALL) -D -m 0644 keyctl_clear.3 $(DESTDIR)$(MAN3)/keyctl_clear.3
	$(INSTALL) -D -m 0644 keyctl_describe.3 $(DESTDIR)$(MAN3)/keyctl_describe.3
	$(LNS) keyctl_describe.3 $(DESTDIR)$(MAN3)/keyctl_describe_alloc.3
	$(INSTALL) -D -m 0644 keyctl_get_keyring_ID.3 $(DESTDIR)$(MAN3)/keyctl_get_keyring_ID.3
	$(INSTALL) -D -m 0644 keyctl_get_security.3 $(DESTDIR)$(MAN3)/keyctl_get_security.3
	$(LNS) keyctl_get_security.3 $(DESTDIR)$(MAN3)/keyctl_get_security_alloc.3
	$(INSTALL) -D -m 0644 keyctl_instantiate.3 $(DESTDIR)$(MAN3)/keyctl_instantiate.3
	$(LNS) keyctl_instantiate.3 $(DESTDIR)$(MAN3)/keyctl_negate.3
	$(LNS) keyctl_instantiate.3 $(DESTDIR)$(MAN3)/keyctl_assume_authority.3
	$(INSTALL) -D -m 0644 keyctl_join_session_keyring.3 $(DESTDIR)$(MAN3)/keyctl_join_session_keyring.3
	$(INSTALL) -D -m 0644 keyctl_link.3 $(DESTDIR)$(MAN3)/keyctl_link.3
	$(LNS) keyctl_link.3 $(DESTDIR)$(MAN3)/keyctl_unlink.3
	$(INSTALL) -D -m 0644 keyctl_read.3 $(DESTDIR)$(MAN3)/keyctl_read.3
	$(LNS) keyctl_read.3 $(DESTDIR)$(MAN3)/keyctl_read_alloc.3
	$(INSTALL) -D -m 0644 keyctl_revoke.3 $(DESTDIR)$(MAN3)/keyctl_revoke.3
	$(INSTALL) -D -m 0644 keyctl_search.3 $(DESTDIR)$(MAN3)/keyctl_search.3
	$(INSTALL) -D -m 0644 keyctl_setperm.3 $(DESTDIR)$(MAN3)/keyctl_setperm.3
	$(INSTALL) -D -m 0644 keyctl_set_reqkey_keyring.3 $(DESTDIR)$(MAN3)/keyctl_set_reqkey_keyring.3
	$(INSTALL) -D -m 0644 keyctl_set_timeout.3 $(DESTDIR)$(MAN3)/keyctl_set_timeout.3
	$(INSTALL) -D -m 0644 keyctl_update.3 $(DESTDIR)$(MAN3)/keyctl_update.3
	$(INSTALL) -D -m 0644 request-key.conf.5 $(DESTDIR)$(MAN5)/request-key.conf.5
	$(INSTALL) -D -m 0644 request-key.8 $(DESTDIR)$(MAN8)/request-key.8
	$(INSTALL) -D -m 0644 keyutils.h $(DESTDIR)$(INCLUDEDIR)/keyutils.h

clean:
	$(RM) libkeyutils*
	$(RM) keyctl request-key
	$(RM) *.o *.os *~
	$(RM) debugfiles.list debugsources.list

show_vars:
	@echo LIBDIR=$(LIBDIR)
	@echo USRLIBDIR=$(USRLIBDIR)
	@echo BUILDFOR=$(BUILDFOR)
