%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}

Name:       qt_qa_engine

Summary:    Automation QA Engine for Sailfish OS
Version:    3.0.0
Release:    1
Group:      Qt/Qt
License:    LGPL3
URL:        https://github.com/coderus/qt_qa_engine
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5XmlPatterns)
BuildRequires:  pkgconfig(systemd)
BuildRequires:  pkgconfig(libshadowutils)
BuildRequires:  pkgconfig(packagekitqt5)
BuildRequires:  pkgconfig(contentaction5)
BuildRequires:  pkgconfig(connman-qt5)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(rpm)
BuildRequires:  qt5-tools
BuildRequires:  qt5-qtdeclarative-devel-tools
BuildRequires:  qt5-plugin-platform-minimal

%description
Library for performing automatic testing Qt/QML applications.

%prep
%setup -q -n %{name}-%{version}

%build

%qtc_qmake5 \
    "PROJECT_PACKAGE_VERSION=%{version}" \
    DEFINES+=Q_OS_SAILFISH
%qtc_make %{?_smp_mflags}

%install
rm -rf %{buildroot}

%qmake5_install

%files
%defattr(-,root,root,-)
%{_libdir}/libqaengine.so
