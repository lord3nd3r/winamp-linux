Name:           winamp
Version:        0.5.0
Release:        beta1%{?dist}
Summary:        Winamp media player for Linux (Qt6 port)
License:        GPLv2
URL:            https://github.com/lord3nd3r/winamp-linux
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.2
BuildRequires:  qt6-qtmultimedia-devel >= 6.2
BuildRequires:  qt6-qttools-devel
BuildRequires:  mesa-libGL-devel
BuildRequires:  libX11-devel
BuildRequires:  libXext-devel
BuildRequires:  zlib-devel
BuildRequires:  libpng-devel
BuildRequires:  libjpeg-turbo-devel

Requires:       qt6-qtbase >= 6.2
Requires:       qt6-qtmultimedia >= 6.2
Requires:       mesa-libGL
Requires:       libprojectM

%description
A native Linux port of the legendary Winamp media player using Qt6.
Features the classic Winamp look and feel with support for classic skins,
equalizer, playlist editor, media library, and MilkDrop visualizations
via projectM.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -G Ninja
%cmake_build

%install
%cmake_install

%files
%license LICENSE.md
%{_bindir}/winamp
%{_datadir}/winamp/
%{_datadir}/applications/winamp.desktop
%{_datadir}/metainfo/org.winamp.Winamp.metainfo.xml
%{_datadir}/icons/hicolor/256x256/apps/winamp.ico

%changelog
* Mon Feb 17 2026 lord3nd3r <lord3nd3r@github.com> - 0.5.0-beta1
- First Linux beta release
