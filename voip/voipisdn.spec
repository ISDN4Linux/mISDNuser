Vendor:       SuSE GmbH, Nuernberg, Germany
Distribution: SuSE Linux 7.3 (i386)
Name:         voipisdn
Packager:     feedback@suse.de
Copyright:    GPL
Group:        Applications/Communications
Provides:     voipisdn
Autoreqprov:  on
Version:      20030423
Release:      1
Summary:      Voice Communication Over Data Networks
Source1:      hisax_voip-%{version}.tar.bz2
Source2:      gsm-1.0.7.tar.gz
BuildRoot:    /var/tmp/%{name}-build

%description
Voice Communication Over Data TCP/IP Networks ISDN gateway

Authors:
--------
    Karsten Keil <kkeil@suse.de>

SuSE series: net

%prep
%setup -T -c -n voipisdn
%setup -n voipisdn -D -T -a 1
%setup -n voipisdn -D -T -a 2

%build

cd gsm-1.0-pl6
make
cd ..

cd hisax
make all

%install
rm -rf RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
install -c -s -m 755 hisax/voip/voipisdn     $RPM_BUILD_ROOT/usr/bin/

%{?suse_check}

%clean
rm -rf RPM_BUILD_ROOT


%files
%defattr(-,root,root)
/usr/bin/*

