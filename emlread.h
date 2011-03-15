/*
    UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
    Copyright (C) 2008-2011 Avi Rozen <avi.rozen@gmail.com>

    DBX file format parsing code is based on DbxConv - a DBX to MBOX
    Converter.  Copyright (C) 2008, 2009 Ulrich Krebs
    <ukrebs@freenet.de>

    RFC-2822 and RFC-2047 parsing code is adapted from GNU Mailutils -
    a suite of utilities for electronic mail, Copyright (C) 2002,
    2003, 2004, 2005, 2006, 2009, 2010 Free Software Foundation, Inc.

    This file is part of UnDBX.

    UnDBX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _EML_READ_H_
#define _EML_READ_H_

#ifdef __cplusplus
extern "C" {
#endif

  #include "dbxsys.h"
  
  void eml_parse(char *message, char **subject, char **from, char **to, time_t *timestamp);

#ifdef __cplusplus
};
#endif

#endif /* _EML_READ_H_ */
