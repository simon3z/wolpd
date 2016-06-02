Name:           wolpd
Version:        0.5.1
Release:        1%{?dist}
Summary:        Wake-On-Lan proxy daemon

Group:          System Environment/Daemons
License:        GPLv3+
URL:            http://github.com/simon3z/wolpd
Source0:        https://github.com/simon3z/%{name}/archive/v%{version}/%{name}-%{version}.tar.gz

%{?systemd_requires}

BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  help2man
BuildRequires:  systemd


%description
Wake-On-Lan proxy daemon.

%prep
%setup -q


%build
test -x ./configure || ./autogen.sh
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
%{__install} -D -m 755 wolpd.service \
    $RPM_BUILD_ROOT/%{_unitdir}/wolpd.service
%{__install} -D -m 755 wolpd.sysconfig \
    $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/wolpd


%clean
rm -rf $RPM_BUILD_ROOT


%post
%systemd_post wolpd.service


%preun
%systemd_preun wolpd.service


%postun
%systemd_postun_with_restart wolpd.service


%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING NEWS ChangeLog README.md
%config(noreplace) %{_sysconfdir}/sysconfig/wolpd
%{_sbindir}/wolpd
%{_unitdir}/wolpd.service
%{_mandir}/man8/wolpd.8*


%changelog
* Thu Jun 2 2016 Federico Simoncelli <federico.simoncelli@gmail.com> 0.5.1-1
- update to 0.5.1

* Fri Feb 12 2010 Federico Simoncelli <federico.simoncelli@gmail.com> 0.5.0-1
- first release
