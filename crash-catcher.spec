Summary: Automatic bug detection and reporting tool
Name: crash-catcher
Version: 0.0.1
Release: 7%{?dist}
License: GPLv2+
Group: Applications/System
URL: https://fedorahosted.org/crash-catcher/
Source: crash-catcher-0.0.1.tar.gz
Source1: crash-catcher.init
BuildRequires: dbus-c++-devel
BuildRequires: gtkmm24-devel
BuildRequires: glib2-devel
BuildRequires: dbus-glib-devel
BuildRequires: rpm-devel >= 4.6
BuildRequires: sqlite-devel > 3.0
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
CrashCatcher is a tool to help users to detect defects in applications and 
to create a bug report with all informations needed by maintainer to fix it. 
It uses plugin system to extend its functionality.

%package applet
Summary: CrashCatcher's applet
Group: User Interface/Desktops
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description applet
Simple systray applet to notify user about new events detected by crash-catcher 
daemon

%package gui
Summary: CrashCatcher's gui
Group: User Interface/Desktops
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description gui
GTK+ wizard for convenient bug reporting.

%package addon-ccpp
Summary: CrashCatcher's C/C++ addon
Group: System Environment/Libraries
License: GPLv2+
Requires: gdb
Requires: %{name} = %{version}-%{release}

%description addon-ccpp
This package contains hook for C/C++ crashed programs and CrashCatcher's C/C++ 
language plugin.

%package plugin-sqlite3
Summary: CrashCatcher's SQLite3 database plugin
Group: System Environment/Libraries
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description plugin-sqlite3
This package contains SQLite3 database plugin. It is used for storing the data 
required for creating a bug report.

%package plugin-logger
Summary: CrashCatcher's logger reporter plugin
Group: System Environment/Libraries
License: GPLv2+
Requires: %{name} = %{version}-%{release}

%description plugin-logger
The simple reporter plugin, which writes a report to a specified file.

%package plugin-mailx
Summary: CrashCatcher's mailx reporter plugin
Group: System Environment/Libraries
License: GPLv2+
Requires: %{name} = %{version}-%{release}
Requires: mailx

%description plugin-mailx
The simple reporter plugin, which sends a report via mailx to a specified email. 

%prep
%setup -q

%build
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

rm -rf $RPM_BUILD_ROOT/%{_libdir}/lib*.la
rm -rf $RPM_BUILD_ROOT/%{_libdir}/crash-catcher/lib*.la
mkdir -p ${RPM_BUILD_ROOT}/etc/rc.d/init.d
install -m 755 %SOURCE1 ${RPM_BUILD_ROOT}/etc/rc.d/init.d/crash-catcher
mkdir -p $RPM_BUILD_ROOT/var/cache/crash-catcher

%clean
rm -rf $RPM_BUILD_ROOT

%post 
/sbin/ldconfig
/sbin/chkconfig --add crash-catcher

%preun
if [ "$1" = 0 ] ; then
  service crash-catcher stop >/dev/null 2>&1
  /sbin/chkconfig --del crash-catcher
fi

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_sbindir}/crash-catcher
%config(noreplace) %{_sysconfdir}/crash-catcher/crash-catcher.conf
%{_libdir}/lib*.so*
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/dbus-crash-catcher.conf
%config /etc/rc.d/init.d/crash-catcher
%dir /var/cache/crash-catcher

%files applet
%defattr(-,root,root,-)
%{_bindir}/cc-applet

%files gui
%defattr(-,root,root,-)
%{_bindir}/cc-gui
%{_datadir}/crash-catcher/*.py*
%{_datadir}/crash-catcher/*.glade

%files addon-ccpp
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/CCpp.conf
%{_libdir}/crash-catcher/libCCpp.so*
%{_libexecdir}/hookCCpp

%files plugin-sqlite3
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/SQLite3.conf
%{_libdir}/crash-catcher/libSQLite3.so*

%files plugin-logger
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/Logger.conf
%{_libdir}/crash-catcher/libLogger.so*

%files plugin-mailx
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/crash-catcher/plugins/Mailx.conf
%{_libdir}/crash-catcher/libMailx.so*

%changelog
* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-7
- Fixed cancel button behaviour in reporter
- disabled core file sending
- removed some debug messages

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-6
- fixed DB path
- added new signals to handler
- gui should survive the dbus timeout

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-5
- fixed catching debuinfo install exceptions
- some gui fixes
- added check for GPGP public key

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-4
- changed from full bt to simple bt

* Thu Feb 26 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-3
- spec file cleanups
- changed default paths to crash DB and log DB
- fixed some memory leaks

* Tue Feb 24 2009 Jiri Moskovcak <jmoskovc@redhat.com> 0.0.1-2
- spec cleanup
- added new subpackage gui

* Wed Feb 18 2009 Zdenek Prikryl <zprikryl@redhat.com> 0.0.1-1
- initial packing
