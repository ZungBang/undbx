/*
    UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
    Copyright (C) 2008-2015 Avi Rozen <avi.rozen@gmail.com>

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

#ifndef _DBX_SYS_H_
#define _DBX_SYS_H_

#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned long long int filetime_t;
  
  char **sys_glob(char *parent, char *pattern, int *num_files);
  void sys_glob_free(char **pglob);
  int sys_mkdir(char *parent, char *dir);
  int sys_chdir(char *dir);
  char *sys_getcwd(void);
  unsigned long long int sys_filesize(char *parent, char *filename);
  int sys_delete(char *parent, char *filename);
  int sys_move(char *parent, char *filename, char *destination);
  int sys_set_time(char *filename, time_t timestamp);
  int sys_set_filetime(char *filename, filetime_t filetime);
  char *sys_basename(char *path);
  char *sys_dirname(char *path);
  size_t sys_fread(void * ptr, size_t size, size_t nitems, FILE * stream);
  void sys_fread_long_long(long long int *value, FILE *file);
  void sys_fread_int(int *value, FILE *file);
  void sys_fread_short(short *value, FILE *file);
  
#ifdef __cplusplus
};
#endif

#endif /* _DBX_SYS_H_ */
