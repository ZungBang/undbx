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

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# define WORDS_BIGENDIAN 1 /* safe default here (see sys_fread_* funcs) */
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>

#include "dbxsys.h"

#define JAN1ST1970 0x19DB1DED53E8000ULL
#define NSPERSEC 1000000000ULL

#ifdef __unix__

#include <glob.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

static char **_sys_glob(char *pattern, int *num_files)
{
  int i = 0;
  char **files = NULL;
  glob_t result;

  glob(pattern, GLOB_MARK, NULL, &result);

  files = (char **)calloc(result.gl_pathc + 1, sizeof(char *));
  for (i = 0 ; i < result.gl_pathc; i++) {
    files[i] = strdup(result.gl_pathv[i]);
  }
  
  if (num_files)
    *num_files = result.gl_pathc;

  globfree(&result);
  return files;
}

static int _sys_mkdir(char *path)
{
  int rc = mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  return (rc == 0 || errno == EEXIST)? 0:rc;
}

static int _sys_chdir(char *dir)
{
  return chdir(dir);
}

static char *_sys_getcwd(void)
{
  return getcwd(NULL, 0);
}

static int _sys_set_time(char *filename, time_t timestamp)
{
  struct utimbuf timbuf;
  timbuf.actime = timestamp;
  timbuf.modtime = timestamp;
  return utime(filename, &timbuf);
}

#endif /* __unix__ */

#ifdef _WIN32

#include <windows.h>
#include <direct.h>
#include <sys/utime.h>

static char **_sys_glob(char *pattern, int *num_files)
{
  char **files = NULL;
  int n = 0;
  WIN32_FIND_DATA f;

  HANDLE h = FindFirstFile(pattern, &f);
  
  if (h != INVALID_HANDLE_VALUE) {

    do {
      files = (char **)realloc(files, sizeof(char *) * (n + 1));
      files[n] = strdup(f.cFileName);
      n++;
    } while (FindNextFile(h, &f));
    
    files = (char **)realloc(files, sizeof(char *) * (n + 1));
    files[n] = NULL;

    FindClose(h);
  }

  if (num_files)
    *num_files = n;
  
  return files;
}

static int _sys_mkdir(char *path)
{
  int rc = _mkdir(path);
  return (rc == 0 || errno == EEXIST)? 0:rc;
}

static int _sys_chdir(char *dir)
{
  int rc;
  /* _chdir doesn't if dir is just C: - must add \ */
  int l = strlen(dir);
  char *path = strdup(dir);
  if (path == NULL)
    return -1;
  if (path[l-1] == ':') {
    path = (char *)realloc(path, l + 2);
    if (path == NULL)
      return -1;
    path[l] = '\\';
    path[l+1] = '\0';
  }
  rc = _chdir(path);
  free(path);
  return rc;
}

static char *_sys_getcwd(void)
{
  return _getcwd(NULL, 0);
}

static int _sys_set_time(char *filename, time_t timestamp)
{
  struct _utimbuf timbuf;
  timbuf.actime = timestamp;
  timbuf.modtime = timestamp;
  return _utime(filename, &timbuf);
}

#endif /* _WIN32 */


char **sys_glob(char *parent, char *pattern, int *num_files)
{
  int rc = 0;
  char *cwd = NULL;
  char **files = NULL;
  *num_files = 0;

  cwd = sys_getcwd();
  if (cwd == NULL)
    return NULL;
  
  rc = sys_chdir(parent);
  if (rc != 0) {
    free(cwd);
    return NULL;
  }
  
  files = _sys_glob(pattern, num_files);
  sys_chdir(cwd);
  free(cwd);

  return files;
}

void sys_glob_free(char **files)
{
  if (files) {
    int i;
    for(i = 0; files[i]; i++) {
      free(files[i]);
      files[i] = NULL;
    }
    free(files);
  }
}

int sys_mkdir(char *parent, char *dir)
{
  int rc = 0;
  char *cwd = NULL;

  rc = _sys_mkdir(parent);
  if (rc != 0)
    return rc;

  cwd = sys_getcwd();
  if (cwd == NULL)
    return -1;
  
  rc = sys_chdir(parent);
  if (rc != 0) {
    free(cwd);
    return -1;
  }
  
  rc = _sys_mkdir(dir);
  sys_chdir(cwd);
  free(cwd);

  return rc;
}

int sys_chdir(char *dir)
{
  if (dir == NULL)
    return -1;
  return _sys_chdir(dir);
}

char *sys_getcwd(void)
{
  return _sys_getcwd();
}

unsigned long long sys_filesize(char *parent, char *filename)
{
  int rc = 0;
  unsigned long long size = 0;
  char *cwd = NULL;
  struct stat buf;

  cwd = sys_getcwd();
  if (cwd == NULL)
    return -1;
  
  rc = sys_chdir(parent);
  if (rc != 0) {
    free(cwd);
    return -1;
  }
  
  rc = stat(filename, &buf);
  size = (rc == 0)? buf.st_size:-1;

  sys_chdir(cwd);
  free(cwd);

  return size;
}

int sys_delete(char *parent, char *filename)
{
  int rc = 0;
  char *cwd = NULL;

  cwd = sys_getcwd();
  if (cwd == NULL)
    return -1;
  
  rc = sys_chdir(parent);
  if (rc != 0) {
    free(cwd);
    return -1;
  }
  
  rc = unlink(filename);
  sys_chdir(cwd);
  free(cwd);

  return rc;
}

int sys_set_time(char *filename, time_t timestamp)
{
  return _sys_set_time(filename, timestamp);
}

int sys_set_filetime(char *filename, filetime_t filetime)
{
  filetime_t t = (filetime - JAN1ST1970) / ((unsigned long long) (NSPERSEC / 100));
  return sys_set_time(filename, (time_t)t);
}

char *sys_basename(char *path)
{
  return basename(path);
}

char *sys_dirname(char *path)
{
  return dirname(path);
}

size_t sys_fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
  return fread(ptr, size, nitems, stream);
}

void sys_fread_long_long(long long *value, FILE *file)
{
#ifndef WORDS_BIGENDIAN
  sys_fread(value, 1, sizeof(long long), file);
#else
  /* the following code is endianness neutral */
  long long llw = 0;
  llw =  (long long) (fgetc(file) & 0xFF);
  llw |= ((long long) (fgetc(file) & 0xFF) << 0x08);
  llw |= ((long long) (fgetc(file) & 0xFF) << 0x10);
  llw |= ((long long) (fgetc(file) & 0xFF) << 0x18);
  llw |= ((long long) (fgetc(file) & 0xFF) << 0x20);
  llw |= ((long long) (fgetc(file) & 0xFF) << 0x28);
  llw |= ((long long) (fgetc(file) & 0xFF) << 0x30);
  llw |= ((long long) (fgetc(file) & 0xFF) << 0x38);  
  *value = llw;
#endif
}

void sys_fread_int(int *value, FILE *file)
{
#ifndef WORDS_BIGENDIAN
  sys_fread(value, 1, sizeof(int), file);
#else
  /* the following code is endianness neutral */
  int dw = 0;
  dw =  (int) (fgetc(file) & 0xFF);
  dw |= ((int) (fgetc(file) & 0xFF) << 0x08);
  dw |= ((int) (fgetc(file) & 0xFF) << 0x10);
  dw |= ((int) (fgetc(file) & 0xFF) << 0x18);
  *value = dw;
#endif
}

void sys_fread_short(short *value, FILE *file)
{
#ifndef WORDS_BIGENDIAN
  sys_fread(value, 1, sizeof(short), file);
#else
  /* the following code is endianness neutral */
  short w = 0;
  w =  (short) (fgetc(file) & 0xFF);
  w |= ((short) (fgetc(file) & 0xFF) << 0x08);
  *value = w;
#endif
}

