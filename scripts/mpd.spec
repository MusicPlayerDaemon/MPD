# the Music Player Daemon (MPD)
# Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
# This project's homepage is: http://www.musicpd.org
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# please send bugfixes or comments to avuton@gmail.com

%define _prefix /usr
%define _share  %_prefix/share
%define _bindir %_prefix/bin
%define _docdir %_share/doc
%define _mandir %_share/man

Summary: Music Player Daemon (MPD)
Name: mpd
Version: 0.13.0
Release: 0
License: GPL
Group: Productivity/Multimedia/Sound/Players
Source: ../%name-%version.tar.gz
#Patch: 
BuildRoot: %{_tmppath}/%{name}-build

%description
Music Player Daemon is simply a daemon for playing music
which can be easily controlled over TCP by different 
clients. It has very low CPU usage and can be controlled
from different machines at the same time.

%prep
%setup -q
#%patch -q1 -b .buildroot

%build
CFLAGS="%{optflags}" \
CXXFLAGS="%{optflags}" \
LDFLAGS="%{optflags}" \
./configure --prefix=%{_prefix}
make

%install
make DESTDIR=$RPM_BUILD_ROOT install

%clean
[ ${RPM_BUILD_ROOT} != "/" ] && rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)

%{_bindir}/%{name}
%doc %{_docdir}
%doc %attr(0444,root,root) %{_mandir}

%changelog
* Fri Jul 21 2006 Avuton Olrich <avuton@gmail.com>
- Initial revision
