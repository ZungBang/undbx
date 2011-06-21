#! /bin/bash

#   UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
#   Copyright (C) 2008-2010 Avi Rozen <avi.rozen@gmail.com>
#
#   DBX file format parsing code is based on DbxConv - a DBX to MBOX
#   Converter.  Copyright (C) 2008, 2009 Ulrich Krebs
#   <ukrebs@freenet.de>
#
#   RFC-2822 and RFC-2047 parsing code is adapted from GNU Mailutils -
#   a suite of utilities for electronic mail, Copyright (C) 2002,
#   2003, 2004, 2005, 2006, 2009, 2010 Free Software Foundation, Inc.
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
# Usage: ./dist-win32.sh [host]
#
# The default host is i686-w64-mingw32 which is the
# MinGW win32 cross-compiler on Debian/testing (my dev box)
# 
# For 64bit windows:
#   export ac_cv_func_realloc_0_nonnull=yes
#   export ac_cv_func_malloc_0_nonnull=yes
#   ./dist-win32.sh x86_64-w64-mingw32
#

win32_host=${1:-i686-w64-mingw32}
basedir=${0/%dist-win32.sh}
cd "$basedir"
autoreconf -vfi && find -maxdepth 1 -size 0 -exec rm -v {} \;
cd -
make distclean
configure="$basedir""configure"
distdir=$(eval "$configure --version" | head -1 | sed s/\ configure\ /-/)
rm -rf ${distdir} ${distdir}.zip* ${distdir}.tar.gz*
[ -f "$basedir"README.rst ] \
    && rst2html "$basedir"README.rst > "$basedir"README.html \
    && wikir "$basedir"README.rst > README.wiki 
mkdir ${distdir} 
win32_install_base=`cd ${distdir} && pwd | sed -e 's,^[^:\\/]:[\\/],/,'` \
    && ${configure} --host=$win32_host --prefix="$win32_install_base" LDFLAGS=-s CFLAGS=-O3 \
    && make dist-gzip \
    && sha1sum ${distdir}.tar.gz > ${distdir}.tar.gz.sha1sum \
    && make \
    && make install \
    && find ${distdir} -type f -exec mv -f {} ${distdir} \; \
    && find ${distdir} -depth -type d -empty -exec rmdir -v {} \; \
    && cp "$basedir"{README.html,COPYING} ${distdir} \
    && zip -rq ${distdir}.zip ${distdir} \
    && sha1sum ${distdir}.zip > ${distdir}.zip.sha1sum \
    && rm -rf ${distdir} \
    && make distclean 
