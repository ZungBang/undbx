#! /bin/bash
#
#   UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
#   Copyright (C) 2008, 2009 Avi Rozen <avi.rozen@gmail.com>
#
#   DBX file format parsing code is based on
#   DbxConv - a DBX to MBOX Converter.
#   Copyright (C) 2008 Ulrich Krebs <ukrebs@freenet.de>
#
#   This file is part of UnDBX.
#
#   UnDBX is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# This is a convenience script that I use to package UnDBX for MS-Windows
#
# Usage: dist-win32.sh [host]
#
# The default host is i586-mingw32msvc which is the
# MinGW win32 cross-compiler on Debian/testing (my dev box)
#

win32_host=${1:-i586-mingw32msvc}
basedir=${0/%dist-win32.sh}
./autogen.sh
make distclean
configure="$basedir""configure"
distdir=$(eval "$configure --version" | head -1 | sed s/\ configure\ /-/)
rm -rf ${distdir} ${distdir}.zip* ${distdir}.tar.gz*
mkdir ${distdir} 
win32_install_base=`cd ${distdir} && pwd | sed -e 's,^[^:\\/]:[\\/],/,'` \
    && ${configure} --host=$win32_host --prefix="$win32_install_base" LDFLAGS=-s CFLAGS=-O3 \
    && make dist-gzip \
    && sha1sum ${distdir}.tar.gz > ${distdir}.tar.gz.sha1sum \
    && make \
    && make install \
    && find ${distdir} -type f -exec mv -f {} ${distdir} \; \
    && find ${distdir} -depth -type d -empty -exec rmdir -v {} \; \
    && cp "$basedir"{README,COPYING} ${distdir} \
    && zip -rq ${distdir}.zip ${distdir} \
    && sha1sum ${distdir}.zip > ${distdir}.zip.sha1sum \
    && rm -rf ${distdir} \
    && make distclean 
