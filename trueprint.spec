Summary: Prints the source files in PostScript
Name: trueprint
Version: 5.2
Release: 1
Source: ftp://ftp.gnu.org/pub/gnu/trueprint/trueprint-%{PACKAGE_VERSION}.tar.gz
Copyright: GPL
Group: Applications/Text
Vendor: Jan "Yenya" Kasprzak <kas@fi.muni.cz>
Packager: Jan "Yenya" Kasprzak <kas@fi.muni.cz>
BuildRoot: /var/tmp/trueprint-root
Prereq: info
Obsoletes: Trueprint
%description

Trueprint is a general purpose program printing program.  It tries to produce
everything that anybody could need in a program printout without the need for
large numbers of switches or pipelines.

Trueprint can currently handle C, C++, Verilog, shell (including ksh),
perl, pascal, listing files and plain text files.

%changelog
* Wed Nov 3 1999 Lezz Giles <gilesfamily@mediaone.net>
- Updated to the 5.2 release.

* Sat Oct 30 1999 Jan "Yenya" Kasprzak <kas@fi.muni.cz>
- Updated to the 5.1 release.
- Strip trueprint (-s added to CFLAGS).

* Tue Jul 27 1999 Jan "Yenya" Kasprzak <kas@fi.muni.cz>
- Initial release

%prep
%setup -n trueprint-%{PACKAGE_VERSION}

%build

CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=/usr
make

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}/usr/{bin,lib,info,man/man1}
make prefix=${RPM_BUILD_ROOT}/usr install
strip ${RPM_BUILD_ROOT}/usr/bin/trueprint

%post
if [ $1 = 1 ]
then
	/sbin/install-info --dir-file=/usr/info/dir /usr/info/trueprint.info
fi

%postun
if [ $1 = 0 ]
then
	/sbin/install-info --dir-file=/usr/info/dir --remove /usr/info/trueprint.info
fi

%files
%attr(0755,root,root) /usr/bin/trueprint
%attr(0644,root,root) %doc /usr/man/man1/trueprint.1
%attr(0644,root,root) %config(noreplace) /usr/lib/printers
%attr(0644,root,root) %doc /usr/info/trueprint.info
%attr(0644,root,root) %doc BUGS NEWS README
