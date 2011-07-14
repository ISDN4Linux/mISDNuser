#                                                     
# spec file for package mISDNuser (Version 2.0.1)    
#                                                     
# Copyright (c) 2009 SUSE LINUX Products GmbH, Nuernberg, Germany.
#                                                                 
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed  
# upon. The license for this file, and modifications and additions to the 
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a     
# license that conforms to the Open Source Definition (Version 1.9)       
# published by the Open Source Initiative.                                

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#                                                                 

# norootforbuild


Name:           mISDNuser
Url:            http://www.misdn.org
License:        GPL v2 only ; LGPL v2.1 only
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
AutoReqProv:    on
BuildRequires:  gcc-c++ glibc-devel libqt4-devel
Group:          Hardware/ISDN
Summary:        Tools and library for mISDN
Version:        2.0.1
Release:        1
Source0:        %{name}-%{version}.tar.bz2
ExcludeArch:    s390 s390x


%description
This package contains libmisdn and some tools to use the mISDN driver.
mISDN is the new modular ISDN driver for Linux.                       



Authors:
--------
    Karsten Keil <kkeil@novell.com>
    Andreas Eversberg <jolly@eversberg.eu>
    Christian Richter <christian.richter@beronet.com>
    Martin Bachem <m.bachem@gmx.de>                  
    Matthias Urlichs <smurf@smurf.noris.de>          
    and more ...                                     

%package devel
License:        GPL v2 only ; LGPL v2.1 only
Requires:       %{name} = %{version}
Requires:	glibc-devel
Summary:        C header files for mISDN
Group:          Development/Libraries/C and C++

%description devel
This package contain the header files and static libraries for
mISDNuser development.



Authors:
--------
    Karsten Keil <kkeil@novell.com>
    Andreas Eversberg <jolly@eversberg.eu>
    Christian Richter <christian.richter@beronet.com>
    Martin Bachem <m.bachem@gmx.de>
    Matthias Urlichs <smurf@smurf.noris.de>
    and more ...


%package gui
License: 	GPL v2 only ; LGPL v2.1 only
Summary:	Qt application to watch the status of mISDN cards
Group:		System/X11/Utilities

%description gui
This subpackage contain a little Qt tool for watching the status of
ISDN cards.

%prep
%setup -q

%build
# This package failed when testing with -Wl,-as-needed being default.
# So we disable it here, if you want to retest, just delete this comment and the line below.
#export SUSE_ASNEEDED=0

aclocal
libtoolize --force --automake --copy
automake --add-missing --copy
autoconf

export CXXFLAGS="$RPM_OPT_FLAGS"
export CFLAGS="$RPM_OPT_FLAGS"

%configure --enable-gui --enable-example

%install
DESTDIR=${RPM_BUILD_ROOT} make install

%post
ldconfig

%postun
ldconfig

%files
%defattr(-,root,root)
%doc NEWS ChangeLog README INSTALL
/usr/bin/l1oipctrl
/usr/bin/misdn_bridge
/usr/bin/misdn_info
/usr/bin/misdn_log
/usr/bin/misdnportinfo
/usr/bin/misdntestcon
/usr/bin/misdntestlayer1
/usr/bin/misdntestlayer3

/usr/sbin/*

%{_libdir}/libmisdn.so.0
%{_libdir}/libmisdn.so.0.2.1

%files devel
%defattr(-,root,root)
%dir /usr/include/mISDN
%attr (0644, root, root) /usr/include/mISDN/*.h
%attr (0644, root, root) %{_libdir}/libmisdn.a
%attr (0644, root, root) %{_libdir}/libmisdn.la
%attr (0644, root, root) %{_libdir}/libmisdn.so

%files gui
%defattr(-,root,root)
/usr/bin/qmisdnwatch

%changelog
* Fri Jun 19 2009 coolo@novell.com
- disable as-needed for this package as it fails to build with it
* Mon Sep  1 2008 kkeil@suse.de
-  first version
