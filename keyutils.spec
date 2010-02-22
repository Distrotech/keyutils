%define vermajor 0
%define version %{vermajor}.3
%define _exec_prefix /

Summary: Linux Key Management Utilities
Name: keyutils
Version: %{version}
Release: 3
License: GPL/LGPL
Group: System Environment/Base
ExclusiveOS: Linux

Source0: http://people.redhat.com/~dhowells/keyutils/keyutils-%{version}.tar.bz2

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
Utilities to control the kernel key management facility and to provide
a mechanism by which the kernel call back to userspace to get a key
instantiated.

%package devel
Summary: Development package for building linux key management utilities
Group: System Environment/Base

%description devel
This package provides headers and libraries for building key utilities.

%prep
%setup -q

%build
make LIBDIR=%{_libdir} RELEASE=.%{release} NO_GLIBC_KEYERR=1

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT LIBDIR=%{_libdir} RELEASE=.%{release} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README LICENCE.GPL LICENCE.LGPL
%{_libdir}/libkeyutil.so.%{version}.%{release}
%{_libdir}/libkeyutil.so.%{vermajor}
/sbin/*
/bin/*
/usr/share/keyutils/*
%{_mandir}/*
%config(noreplace) /etc/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/libkeyutil.so
%{_includedir}/*

%changelog
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
