#     UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
#     Copyright (C) 2008 Avi Rozen <avi.rozen@gmail.com>
#
#     DBX file format parsing code is based on
#     DbxConv - a DBX to MBOX Converter.
#     Copyright (C) 2008 Ulrich Krebs <ukrebs@freenet.de>
#
#     This file is part of UnDBX.
#
#     UnDBX is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.

VERSION=0.12
UNDBX=undbx-$(VERSION)
PACKAGE=$(UNDBX).zip
MD5SUM=$(PACKAGE).md5sum
SRCS=undbx.c dbxsys.c dbxread.c
FILES=undbx.exe README COPYING
HDRS=dbxsys.h dbxread.h
CPPFLAGS+=-DDBX_VERSION=\"$(VERSION)\"
CFLAGS+=-Wall -O3 
CFLAGS_LINUX=-g
CFLAGS_WINDOWS=-s

all: undbx $(MD5SUM)

clean:
	rm -rf undbx undbx.exe $(PACKAGE) $(MD5SUM) $(UNDBX)

undbx: $(SRCS) $(HDRS) Makefile
	gcc -o undbx $(CPPFLAGS) $(CFLAGS) $(CFLAGS_LINUX) $(LDFLAGS) $(SRCS)

undbx.exe: $(SRCS) $(HDRS) Makefile
	i586-mingw32msvc-gcc -o undbx.exe $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WINDOWS) $(LDFLAGS) $(SRCS)

$(PACKAGE): $(FILES)
	mkdir -p $(UNDBX)
	cp -a $(FILES) $(UNDBX)
	zip $(PACKAGE) $(UNDBX)/*
	rm -rf $(UNDBX)

$(MD5SUM): $(PACKAGE)
	md5sum $(PACKAGE) > $(MD5SUM)

