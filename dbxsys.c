/*
    UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
    Copyright (C) 2008 Avi Rozen <avi.rozen@gmail.com>

    DBX file format parsing code is based on
    DbxConv - a DBX to MBOX Converter.
    Copyright (C) 2008 Ulrich Krebs <ukrebs@freenet.de>

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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "dbxsys.h"

#ifdef __unix__

#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

static long _sys_filesize(char *path)
{
  struct stat buf;
  int rc = stat(path, &buf);
  if (rc != 0) {
    return -1;
  }
  return buf.st_size;
}

#endif /* __unix__ */

#ifdef _WIN32

#include <windows.h>
#include <direct.h>

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
  return _chdir(dir);
}

static char *_sys_getcwd(void)
{
  return _getcwd(NULL, 0);
}

static long _sys_filesize(char *path)
{
  BOOL rc;
  WIN32_FILE_ATTRIBUTE_DATA file_info;

  rc = GetFileAttributesEx(path, GetFileExInfoStandard, (void*) &file_info);
  if (!rc)
    return -1;
  return (long) file_info.nFileSizeLow;
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
  return _sys_chdir(dir);
}

char *sys_getcwd(void)
{
  return _sys_getcwd();
}

long sys_filesize(char *parent, char *filename)
{
  int rc = 0;
  long size = 0;
  char *cwd = NULL;

  cwd = sys_getcwd();
  if (cwd == NULL)
    return -1;
  
  rc = sys_chdir(parent);
  if (rc != 0) {
    free(cwd);
    return -1;
  }
  
  size = _sys_filesize(filename);
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
