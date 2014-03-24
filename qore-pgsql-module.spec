%define mod_ver 2.4
%define module_api %(qore --latest-module-api 2>/dev/null)
%define module_dir %{_libdir}/qore-modules

%if 0%{?sles_version}

%define dist .sles%{?sles_version}

%else
%if 0%{?suse_version}

# get *suse release major version
%define os_maj %(echo %suse_version|rev|cut -b3-|rev)
# get *suse release minor version without trailing zeros
%define os_min %(echo %suse_version|rev|cut -b-2|rev|sed s/0*$//)

%if %suse_version > 1010
%define dist .opensuse%{os_maj}_%{os_min}
%else
%define dist .suse%{os_maj}_%{os_min}
%endif

%endif
%endif

# see if we can determine the distribution type
%if 0%{!?dist:1}
%define rh_dist %(if [ -f /etc/redhat-release ];then cat /etc/redhat-release|sed "s/[^0-9.]*//"|cut -f1 -d.;fi)
%if 0%{?rh_dist}
%define dist .rhel%{rh_dist}
%else
%define dist .unknown
%endif
%endif

Summary: PostgreSQL DBI module for Qore
Name: qore-pgsql-module
Version: %{mod_ver}
Release: 1%{dist}
License: LGPL
Group: Development/Languages/Other
URL: http://www.qoretechnologies.com/qore
Source: http://prdownloads.sourceforge.net/qore/%{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: /usr/bin/env
Requires: qore-module-api-%{module_api}
BuildRequires: gcc-c++
BuildRequires: qore-devel >= 0.8.5
BuildRequires: postgresql-devel
BuildRequires: qore
BuildRequires: openssl-devel

%description
PostgreSQL DBI driver module for the Qore Programming Language. The PostgreSQL
driver is character set aware, supports multithreading, transaction management,
stored prodedure and function execution, etc.


%if 0%{?suse_version}
%debug_package
%endif

%prep
%setup -q
./configure RPM_OPT_FLAGS="$RPM_OPT_FLAGS" --prefix=/usr --disable-debug

%build
%{__make}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{module_dir}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/qore-pgsql-module
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{module_dir}
%doc COPYING.LGPL COPYING.MIT README RELEASE-NOTES ChangeLog AUTHORS


%package doc
Summary: PostgreSQL DBI module for Qore
Group: Development/Languages

%description doc
PostgreSQL module for the Qore Programming Language.

This RPM provides API documentation, test and example programs

%files doc
%defattr(-,root,root,-)
%doc docs/pgsql/html test/db-test.q test/sql-stmt.q

%changelog
* Mon Mar 24 2014 David Nichols <david@qore.org> 2.4
- updated version to 2.4

* Wed Sep 18 2013 David Nichols <david@qore.org> 2.3
- updated version to 2.3

* Tue May 14 2013 David Nichols <david@qore.org> 2.2
- updated version to 2.2

* Mon Dec 10 2012 David Nichols <david@qore.org> 2.1
- updated version to 2.1

* Thu Sep 6 2012 David Nichols <david@qore.org> 2.0
- updated version to 2.0

* Thu Nov 24 2011 Petr Vanek <petr@scribus.info> 1.0.6
- updated version to 1.0.6

* Wed Nov 11 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated version to 1.0.5

* Thu Jun 18 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated version to 1.0.4

* Wed Mar 11 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated version to 1.0.3

* Sat Jan 3 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated version to 1.0.2

* Tue Sep 2 2008 David Nichols <david_nichols@users.sourceforge.net>
- initial spec file for separate pgsql release
