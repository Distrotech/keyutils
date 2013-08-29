%define vermajor 1
%define verminor 5.6
%define version %{vermajor}.%{verminor}
%define libdir /%{_lib}
%define usrlibdir %{_prefix}/%{_lib}
%define libapivermajor 1
%define libapiversion %{libapivermajor}.4

# % define buildid .local

Summary: Linux Key Management Utilities
Name: keyutils
Version: %{version}
Release: 1%{?buildid}%{?dist}
License: GPLv2+ and LGPLv2+
Group: System Environment/Base
ExclusiveOS: Linux
Url: http://people.redhat.com/~dhowells/keyutils/

Source0: http://people.redhat.com/~dhowells/keyutils/keyutils-%{version}.tar.bz2

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: glibc-kernheaders >= 2.4-9.1.92
Requires: keyutils-libs == %{version}-%{release}

%description
Utilities to control the kernel key management facility and to provide
a mechanism by which the kernel call back to user space to get a key
instantiated.

%package libs
Summary: Key utilities library
Group: System Environment/Base

%description libs
This package provides a wrapper library for the key management facility system
calls.

%package libs-devel
Summary: Development package for building Linux key management utilities
Group: System Environment/Base
Requires: keyutils-libs == %{version}-%{release}

%description libs-devel
This package provides headers and libraries for building key utilities.

%prep
%setup -q

%build
make \
	NO_ARLIB=1 \
	LIBDIR=%{libdir} \
	USRLIBDIR=%{usrlibdir} \
	RELEASE=.%{release} \
	NO_GLIBC_KEYERR=1 \
	CFLAGS="-Wall $RPM_OPT_FLAGS -Werror"

%install
rm -rf $RPM_BUILD_ROOT
make \
	NO_ARLIB=1 \
	DESTDIR=$RPM_BUILD_ROOT \
	LIBDIR=%{libdir} \
	USRLIBDIR=%{usrlibdir} \
	install

%clean
rm -rf $RPM_BUILD_ROOT

%post libs -p /sbin/ldconfig
%postun libs -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc README LICENCE.GPL
/sbin/*
/bin/*
/usr/share/keyutils
%{_mandir}/man1/*
%{_mandir}/man5/*
%{_mandir}/man8/*
%config(noreplace) /etc/*

%files libs
%defattr(-,root,root,-)
%doc LICENCE.LGPL
%{libdir}/libkeyutils.so.%{libapiversion}
%{libdir}/libkeyutils.so.%{libapivermajor}

%files libs-devel
%defattr(-,root,root,-)
%{usrlibdir}/libkeyutils.so
%{_includedir}/*
%{_mandir}/man3/*

%changelog
* Thu Aug 29 2013 David Howells <dhowells@redhat.com> - 1.5.6-1
- Fix the request-key.conf.5 manpage.
- Fix the max depth of key tree dump (keyctl show).
- The input buffer size for keyctl padd and pinstantiate should be larger.
- Add keyctl_invalidate.3 manpage.

* Wed Nov 30 2011 David Howells <dhowells@redhat.com> - 1.5.5-1
- Fix a Makefile error.

* Wed Nov 30 2011 David Howells <dhowells@redhat.com> - 1.5.4-1
- Fix the keyctl padd command and similar to handle binary input.
- Make keyctl show able to take a keyring to dump.
- Make keyctl show able to take a flag to request hex key IDs.
- Make keyctl show print the real ID of the root keyring.

* Tue Nov 15 2011 David Howells <dhowells@redhat.com>
- Allow /sbin/request-key to have multiple config files.

* Wed Aug 31 2011 David Howells <dhowells@redhat.com>
- Adjust the manual page for 'keyctl unlink' to show keyring is optional.
- Add --version support for the keyutils version and build date.

* Thu Aug 11 2011 David Howells <dhowells@redhat.com> - 1.5.3-1
- Make the keyutils rpm depend on the same keyutils-libs rpm version.

* Tue Jul 26 2011 David Howells <dhowells@redhat.com> - 1.5.2-1
- Use correct format spec for printing pointer subtraction results.

* Tue Jul 19 2011 David Howells <dhowells@redhat.com> - 1.5.1-1
- Fix unread variables.
- Licence file update.

* Thu Mar 10 2011 David Howells <dhowells@redhat.com> - 1.5-1
- Disable RPATH setting in Makefile.
- Add -I. to build to get this keyutils.h.
- Make CFLAGS override on make command line work right.
- Make specfile UTF-8.
- Support KEYCTL_REJECT.
- Support KEYCTL_INSTANTIATE_IOV.
- Add AFSDB DNS lookup program from Wang Lei.
- Generalise DNS lookup program.
- Add recursive scan utility function.
- Add bad key reap command to keyctl.
- Add multi-unlink variant to keyctl unlink command.
- Add multi key purger command to keyctl.
- Handle multi-line commands in keyctl command table.
- Move the package to version to 1.5.

* Tue Mar 1 2011 David Howells <dhowells@redhat.com> - 1.4-4
- Make build guess at default libdirs and word size.
- Make program build depend on library in Makefile.
- Don't include $(DESTDIR) in MAN* macros.
- Remove NO_GLIBC_KEYSYS as it is obsolete.
- Have Makefile extract version info from specfile and version script.
- Provide RPM build rule in Makefile.
- Provide distclean rule in Makefile.

* Fri Dec 17 2010 Diego Elio Pettenò <flameeyes@hosting.flameeyes.eu> - 1.4-3
- Fix local linking and RPATH.

* Thu Jun 10 2010 David Howells <dhowells@redhat.com> - 1.4-2
- Fix prototypes in manual pages (some char* should be void*).
- Rename the keyctl_security.3 manpage to keyctl_get_security.3.

* Fri Mar 19 2010 David Howells <dhowells@redhat.com> - 1.4-1
- Fix the library naming wrt the version.
- Move the package to version to 1.4.

* Fri Mar 19 2010 David Howells <dhowells@redhat.com> - 1.3-3
- Fix spelling mistakes in manpages.
- Add an index manpage for all the keyctl functions.

* Thu Mar 11 2010 David Howells <dhowells@redhat.com> - 1.3-2
- Fix rpmlint warnings.

* Fri Feb 26 2010 David Howells <dhowells@redhat.com> - 1.3-1
- Fix compiler warnings in request-key.
- Expose the kernel function to get a key's security context.
- Expose the kernel function to set a processes keyring onto its parent.
- Move libkeyutils library version to 1.3.

* Tue Aug 22 2006 David Howells <dhowells@redhat.com> - 1.2-1
- Remove syscall manual pages (section 2) to man-pages package [BZ 203582]
- Don't write to serial port in debugging script

* Mon Jun 5 2006 David Howells <dhowells@redhat.com> - 1.1-4
- Call ldconfig during (un)installation.

* Fri May 5 2006 David Howells <dhowells@redhat.com> - 1.1-3
- Don't include the release number in the shared library filename
- Don't build static library

* Fri May 5 2006 David Howells <dhowells@redhat.com> - 1.1-2
- More bug fixes from Fedora reviewer.

* Thu May 4 2006 David Howells <dhowells@redhat.com> - 1.1-1
- Fix rpmlint errors

* Mon Dec 5 2005 David Howells <dhowells@redhat.com> - 1.0-2
- Add build dependency on glibc-kernheaders with key management syscall numbers

* Tue Nov 29 2005 David Howells <dhowells@redhat.com> - 1.0-1
- Add data pipe-in facility for keyctl request2

* Mon Nov 28 2005 David Howells <dhowells@redhat.com> - 1.0-1
- Rename library and header file "keyutil" -> "keyutils" for consistency
- Fix shared library version naming to same way as glibc.
- Add versioning for shared library symbols
- Create new keyutils-libs package and install library and main symlink there
- Install base library symlink in /usr/lib and place in devel package
- Added a keyutils archive library
- Shorten displayed key permissions list to just those we actually have

* Thu Nov 24 2005 David Howells <dhowells@redhat.com> - 0.3-4
- Add data pipe-in facilities for keyctl add, update and instantiate

* Fri Nov 18 2005 David Howells <dhowells@redhat.com> - 0.3-3
- Added stdint.h inclusion in keyutils.h
- Made request-key.c use request_key() rather than keyctl_search()
- Added piping facility to request-key

* Thu Nov 17 2005 David Howells <dhowells@redhat.com> - 0.3-2
- Added timeout keyctl option
- request_key auth keys must now be assumed
- Fix keyctl argument ordering for debug negate line in request-key.conf

* Thu Jul 28 2005 David Howells <dhowells@redhat.com> - 0.3-1
- Must invoke initialisation from perror() override in libkeyutils
- Minor UI changes

* Wed Jul 20 2005 David Howells <dhowells@redhat.com> - 0.2-2
- Bump version to permit building in main repositories.

* Tue Jul 12 2005 David Howells <dhowells@redhat.com> - 0.2-1
- Don't attempt to define the error codes in the header file.
- Pass the release ID through to the makefile to affect the shared library name.

* Tue Jul 12 2005 David Howells <dhowells@redhat.com> - 0.1-3
- Build in the perror() override to get the key error strings displayed.

* Tue Jul 12 2005 David Howells <dhowells@redhat.com> - 0.1-2
- Need a defattr directive after each files directive.

* Tue Jul 12 2005 David Howells <dhowells@redhat.com> - 0.1-1
- Package creation.
