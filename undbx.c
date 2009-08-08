/*
    UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
    Copyright (C) 2008, 2009 Avi Rozen <avi.rozen@gmail.com>

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

#ifdef HAVE_CONFIG_H
# include <config.h>
# define DBX_VERSION PACKAGE_VERSION
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "dbxread.h"

typedef enum { DBX_SAVE_NOOP, DBX_SAVE_OK, DBX_SAVE_ERROR } dbx_save_status_t;

static int _str_cmp(const char **ia, const char **ib)
{
  return strcmp(*ia, *ib);
}

static dbx_save_status_t _save_message(char *dir, char *filename, char *message, long size)
{
  FILE *eml = NULL;
  char *cwd = NULL;
  size_t b = 0;
  int rc = 0;

  cwd = sys_getcwd();
  if (cwd == NULL) {
    perror("_save_message (sys_getcwd)");
    return DBX_SAVE_ERROR;
  }
  
  rc = sys_chdir(dir);
  if (rc != 0) {
    perror("_save_message (sys_chdir)");
    return DBX_SAVE_ERROR;
  }

  eml = fopen(filename, "wb");

  sys_chdir(cwd);
  free(cwd);
  cwd = NULL;
  
  if (eml == NULL) {
    perror("_save_message (fopen)");    
    return DBX_SAVE_ERROR;
  }

  b = fwrite(message, 1, size, eml);
  if (b != size) {
    perror("_save_message (fwrite)");
    return DBX_SAVE_ERROR;
  }

  fclose(eml);
  return DBX_SAVE_OK;
}


static void _set_message_date(dbx_info_t *info, char *dir)
{
  char *cwd = NULL;
  int rc = 0;
  filetime_t filetime = 0;

  cwd = sys_getcwd();
  if (cwd == NULL) {
    perror("_set_message_date (sys_getcwd)");
    return;
  }
  
  rc = sys_chdir(dir);
  if (rc != 0) {
    perror("_set_message_date (sys_chdir)");
    return;
  }

  if (info->send_create_time && info->receive_create_time)
    filetime = info->send_create_time < info->receive_create_time ?
      info->send_create_time : info->receive_create_time;
  else
    filetime = info->send_create_time? info->send_create_time : info->receive_create_time;

  sys_set_filetime(info->filename, filetime);

  sys_chdir(cwd);
  free(cwd);
  cwd = NULL;
}

static dbx_save_status_t _maybe_save_message(dbx_t *dbx, int imessage, char *dir, int force)
{
  dbx_save_status_t status = DBX_SAVE_NOOP;
  dbx_info_t *info = dbx->info + imessage;
  unsigned long long size = 0;
  unsigned int message_size = 0;
  char *message = NULL;

  if (!force) 
    size = sys_filesize(dir, info->filename);
  
  if (force || (info->valid & DBX_MASK_MSGSIZE) == 0 || size != info->message_size) {
    message = dbx_message(dbx, imessage, &message_size);
    if (force || (size != message_size)) {
      status = _save_message(dir, info->filename, message, message_size);
      switch (status) {
      case DBX_SAVE_ERROR:
        printf("%5.1f%%  ERROR: %s\n", 100 * (imessage + 1.0)/dbx->message_count, info->filename);
        fflush(stdout);
        break;
      case DBX_SAVE_OK:
        _set_message_date(info, dir);
        printf("%5.1f%%     OK: %s\n", 100 * (imessage + 1.0)/dbx->message_count, info->filename);
        fflush(stdout);
        break;
      default:
        break;
      }
    }
    free(message);
  }

  return status;
}

static int _undbx(char *dbx_dir, char *out_dir, char *dbx_file)
{
  int deleted = 0;
  int saved = 0;
  int errors = 0;
  
  int no_more_messages = 0;
  int no_more_files = 0;
  char **eml_files = NULL;
  int num_eml_files = 0;
  int imessage = 0;
  int ifile = 0;
  dbx_t *dbx = NULL;
  char *eml_dir = NULL;
  char *cwd = NULL;
  int rc = 0;

  cwd = sys_getcwd();
  if (cwd == NULL) {
    fprintf(stderr, "error: can't get current working directory\n");
    return -1;
  }

  rc = sys_chdir(dbx_dir);
  if (rc != 0) {
    fprintf(stderr, "error: can't chdir to %s\n", dbx_dir);
    return -1;
  }
  
  dbx = dbx_open(dbx_file);

  sys_chdir(cwd);

  if (dbx == NULL) {
    fprintf(stderr, "warning: can't open DBX file %s\n", dbx_file);
    return -1;
  }

  if (dbx->type != DBX_TYPE_EMAIL) {
    fprintf(stderr, "warning: DBX file %s does not contain messages\n", dbx_file);
    return -1;
  }

  if (dbx->file_size >= 0x80000000) {
    fprintf(stderr, "warning: DBX file %s is corrupted (larger than 2GB)\n", dbx_file);
  }

  eml_dir = strdup(dbx_file);
  eml_dir[strlen(eml_dir) - 4] = '\0';
  rc = sys_mkdir(out_dir, eml_dir);
  if (rc != 0) {
    fprintf(stderr, "error: can't create directory %s/%s\n", out_dir, eml_dir);
    return -1;
  }

  printf("------ Extracting %d messages from %s to %s/%s ... \n", dbx->message_count, dbx_file, out_dir, eml_dir);
  fflush(stdout);

  rc = sys_chdir(out_dir);
  if (rc != 0) {
    fprintf(stderr, "error: can't chdir to %s\n", out_dir);
    return -1;
  }

  eml_files = sys_glob(eml_dir, "*.eml", &num_eml_files);

  no_more_messages = (imessage == dbx->message_count);
  no_more_files = (ifile == num_eml_files);
        
  qsort(eml_files, num_eml_files, sizeof(char *), (dbx_cmpfunc_t) _str_cmp);
      
  while (!no_more_messages || !no_more_files) {
    
    dbx_save_status_t status = DBX_SAVE_NOOP;
    int cond;
    
    if (!no_more_messages && !no_more_files) {
      cond = strcmp(dbx->info[imessage].filename, eml_files[ifile]);
    }
    else if (no_more_files)
      cond = -1;
    else
      cond = 1;
      
    if (cond < 0) {
      /* message not found on disk: extract from dbx */
      status = _maybe_save_message(dbx, imessage, eml_dir, 1); 
      imessage++;
      no_more_messages = (imessage == dbx->message_count);
    }
    else if (cond == 0) {
      /* message found on disk: extract from dbx if modified */
      status = _maybe_save_message(dbx, imessage, eml_dir, 0);
      imessage++;
      no_more_messages = (imessage == dbx->message_count);      
      ifile++;
      no_more_files = (ifile == num_eml_files);
    }
    else {
      /* file on disk not found in dbx: delete from disk */
      sys_delete(eml_dir, eml_files[ifile]);
      printf("%5.1f%% DELETE: %s\n", 100 * (imessage + 1.0)/dbx->message_count, eml_files[ifile]);
      ifile++;
      no_more_files = (ifile == num_eml_files);
      deleted++;
    }

    if (status == DBX_SAVE_OK)
      saved++;
    if (status == DBX_SAVE_ERROR)
      errors++;
  }

  printf("------ %d messages saved, %d skipped, %d errors, %d files deleted\n", saved, dbx->message_count - saved - errors, errors, deleted);
  fflush(stdout);
  
  sys_glob_free(eml_files);

  free(eml_dir);
  eml_dir = NULL;
  
  dbx_close(dbx);

  sys_chdir(cwd);
  free(cwd);
  cwd = NULL;

  return 0;
}

static char **_get_files(char **dir, int *num_files)
{
  char **files = NULL;
  int l = strlen(*dir);
  
  files = sys_glob(*dir, "*.dbx", num_files);
  if (*num_files == 0 && l > 4 && strcasecmp(*dir + l - 4, ".dbx") == 0) {
    char *dirname = NULL;
    sys_glob_free(files);
    files = (char **)calloc(2, sizeof(char *));
    files[0] = strdup(sys_basename(*dir));
    *num_files = 1;
    dirname = strdup(sys_dirname(*dir));
    free(*dir);
    *dir = dirname;
  }
  return files;
}

static void _usage(char *prog)
{
  fprintf(stderr, "Usage: %s <DBX-DIRECTORY | DBX-FILE> [<OUTPUT-DIRECTORY>]\n", prog);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
  int n = 0;
  int fail = 0;
  char **dbx_files = NULL;
  char *dbx_dir = NULL;
  char *out_dir = NULL;
  int num_dbx_files = 0;

  printf("UnDBX v" DBX_VERSION " (" __DATE__ ")\n");
  
  if (argc < 2 || argc > 3)
    _usage(argv[0]);

  dbx_dir = strdup(argv[1]);
  
  if (argc == 3)
    out_dir = argv[2];
  else
    out_dir = ".";

  dbx_files = _get_files(&dbx_dir, &num_dbx_files);
  for(n = 0; n < num_dbx_files; n++) {
    if (_undbx(dbx_dir, out_dir, dbx_files[n]))
      fail++;
  }

  sys_glob_free(dbx_files);
  free(dbx_dir);

  printf("Extracted %d out of %d DBX files.\n", n - fail, n);
  
  return EXIT_SUCCESS;
}
