%define vermajor 0
%define version %{vermajor}.1
%define _exec_prefix /

Summary: Linux Key Management Utilities
Name: keyutils
Version: %{version}
Release: 1
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
make LIBDIR=%{_libdir}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT LIBDIR=%{_libdir} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README LICENCE.GPL LICENCE.LGPL
%{_libdir}/libkeyutil.so.%{version}
%{_libdir}/libkeyutil.so.%{vermajor}
/sbin/*
/bin/*
/usr/share/keyutils/*
%{_mandir}/*
%config(noreplace) /etc/*

%files devel
%{_libdir}/libkeyutil.so
%{_includedir}/*

%changelog
* Mon Jul 12 2005 David Howells <dhowells@redhat.com> - 0.1-1
- Package creation.
