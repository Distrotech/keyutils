%define vermajor 1
%define version %{vermajor}.4
%define libdir /%{_lib}
%define usrlibdir %{_prefix}/%{_lib}
%define libapivermajor 1
%define libapiversion %{libapivermajor}.3

Summary: Linux Key Management Utilities
Name: keyutils
Version: %{version}
Release: 1%{?dist}
License: GPLv2+ and LGPLv2+
Group: System Environment/Base
ExclusiveOS: Linux
Url: http://people.redhat.com/~dhowells/keyutils/

Source0: http://people.redhat.com/~dhowells/keyutils/keyutils-%{version}.tar.bz2

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: glibc-kernheaders >= 2.4-9.1.92

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
	CFLAGS="-Wall $RPM_OPT_FLAGS"

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
* Fri Mar 19 2010 David Howells  <dhowells@redhat.com> - 1.4-1
- Fix the library naming wrt the version.
- Move the package to version to 1.4.

* Fri Mar 19 2010 David Howells  <dhowells@redhat.com> - 1.3-3
- Fix spelling mistakes in manpages.
- Add an index manpage for all the keyctl functions.

* Thu Mar 11 2010 David Howells  <dhowells@redhat.com> - 1.3-2
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

* Mon Jul 12 2005 David Howells <dhowells@redhat.com> - 0.2-1
- Don't attempt to define the error codes in the header file.
- Pass the release ID through to the makefile to affect the shared library name.

* Mon Jul 12 2005 David Howells <dhowells@redhat.com> - 0.1-3
- Build in the perror() override to get the key error strings displayed.

* Mon Jul 12 2005 David Howells <dhowells@redhat.com> - 0.1-2
- Need a defattr directive after each files directive.

* Mon Jul 12 2005 David Howells <dhowells@redhat.com> - 0.1-1
- Package creation.
