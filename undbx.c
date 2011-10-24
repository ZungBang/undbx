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
# define DBX_VERSION PACKAGE_VERSION
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include "dbxread.h"

typedef enum { DBX_SAVE_NOOP, DBX_SAVE_OK, DBX_SAVE_ERROR } dbx_save_status_t;
typedef enum { DBX_EXTRACT_IGNORE, DBX_EXTRACT_FORCE, DBX_EXTRACT_MAYBE } dbx_extract_decision_t;

static int _str_cmp(const char **ia, const char **ib)
{
  return strcmp(*ia, *ib);
}

static int _dbx_offset_cmp(const dbx_info_t *ia, const dbx_info_t *ib)
{
  return (ia->offset - ib->offset);
}

static dbx_save_status_t _save_message(char *dir, char *filename, char *message, unsigned int size)
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

  eml = fopen(filename, "w+b");

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


static void _set_message_time(char *dir, char *filename, time_t timestamp)
{
  char *cwd = NULL;
  int rc = 0;

  cwd = sys_getcwd();
  if (cwd == NULL) {
    perror("_set_message_time (sys_getcwd)");
    return;
  }
  
  rc = sys_chdir(dir);
  if (rc != 0) {
    perror("_set_message_time (sys_chdir)");
    return;
  }

  sys_set_time(filename, timestamp);

  sys_chdir(cwd);
  free(cwd);
  cwd = NULL;
}

static void _set_message_filetime(dbx_info_t *info, char *dir)
{
  char *cwd = NULL;
  int rc = 0;
  filetime_t filetime = 0;

  cwd = sys_getcwd();
  if (cwd == NULL) {
    perror("_set_message_filetime (sys_getcwd)");
    return;
  }
  
  rc = sys_chdir(dir);
  if (rc != 0) {
    perror("_set_message_filetime (sys_chdir)");
    return;
  }

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
  unsigned long long int size = 0;
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
        dbx_progress_update(dbx->progress_handle, DBX_STATUS_ERROR, imessage, "%s", info->filename);
        break;
      case DBX_SAVE_OK:
        _set_message_filetime(info, dir);
        dbx_progress_update(dbx->progress_handle, DBX_STATUS_OK, imessage, "%s", info->filename);
        break;
      default:
        break;
      }
    }
    free(message);
  }

  return status;
}


static void _recover(dbx_t *dbx, char *out_dir, char *eml_dir, int *saved, int *errors)
{
  int i = 0;
  const char *scan_type[2] = { "messages", "deleted message fragments" };
  
  for(i = 0; i < dbx->scan_count; i++) {
    int s = 0;
    int e = 0;
    dbx_save_status_t status = DBX_SAVE_NOOP;
    int imessage = 0;
    char *message = NULL;
    char *filename = NULL;
    unsigned int size = 0;
    time_t timestamp = 0;
    
    if (dbx->scan[i].count > 0) {
      char *dest_dir = strdup(eml_dir);
      if (dbx->scan[i].deleted) {
        dest_dir = (char *)realloc(dest_dir, sizeof(char) * (strlen(dest_dir) + strlen("/deleted") + 1));
        strcat(dest_dir, "/deleted");
      }

      dbx_progress_push(dbx->progress_handle,
                        DBX_VERBOSITY_INFO,
                        dbx->scan[i].count,
                        "Recovering %d %s with offset %Ld from %s to %s/%s",
                        dbx->scan[i].count,
                        scan_type[dbx->scan[i].deleted],
                        dbx->scan[i].offset,
                        dbx->filename,
                        out_dir,
                        dest_dir);
      if (dbx->scan[i].deleted) {
        int rc = sys_mkdir(eml_dir, "deleted");
        if (rc != 0) {
          perror("_recover (sys_mkdir)");
          break;
        }
      }
      for (imessage = 0; imessage < dbx->scan[i].count; imessage++) {
        message = dbx_recover_message(dbx, i, imessage, &size, &timestamp, &filename);
        if (message) {
          status = _save_message(dest_dir, filename, message, size);
          switch (status) {
          case DBX_SAVE_ERROR:
            e++;
            dbx_progress_update(dbx->progress_handle, DBX_STATUS_ERROR, imessage, "%s", filename);
            break;
          case DBX_SAVE_OK:
            s++;
            _set_message_time(dest_dir, filename, timestamp);
            dbx_progress_update(dbx->progress_handle, DBX_STATUS_OK, imessage, "%s", filename);
            break;
          default:
            break;
          }
        }
        free(filename);
        free(message);
      }
      free(dest_dir);
      dbx_progress_pop(dbx->progress_handle,
                       "%d %s recovered, %d errors",
                       s,
                       scan_type[dbx->scan[i].deleted],
                       e);
    }
    *saved += s;
    *errors += e;
  }
}

static void _extract(dbx_t *dbx, char *out_dir, char *eml_dir, int *saved, int *deleted, int *errors)
{
  int no_more_messages = 0;
  int no_more_files = 0;
  char **eml_files = NULL;
  int num_eml_files = 0;
  int imessage = 0;
  int ifile = 0;
  
  dbx_progress_push(dbx->progress_handle,
                    DBX_VERBOSITY_INFO,
                    dbx->message_count,
                    "Extracting %d messages from %s to %s/%s",
                    dbx->message_count,
                    dbx->filename,
                    out_dir,
                    eml_dir);

  eml_files = sys_glob(eml_dir, "*.eml", &num_eml_files);

  no_more_messages = (imessage == dbx->message_count);
  no_more_files = (ifile == num_eml_files);
  
  qsort(eml_files, num_eml_files, sizeof(char *), (dbx_cmpfunc_t) _str_cmp);
      
  if (dbx->options->keep_deleted) {
    int rc = sys_mkdir(eml_dir, "deleted");
    if (rc != 0) {
      perror("_extract (sys_mkdir)");
      return;
    }
  }

  while (!no_more_messages || !no_more_files) {
      
    int cond;
    int ignore;

    ignore = (dbx->options->ignore0 &&
              !no_more_messages &&
              dbx->info[imessage].offset == 0);
    
    if (!no_more_messages && !no_more_files) {
      cond = strcmp(dbx->info[imessage].filename, eml_files[ifile]);
      if (ignore && cond == 0) {
        cond = 1;
        imessage++;
      }
    }
    else if (no_more_files)
      cond = -1;
    else
      cond = 1;

    dbx->info[imessage].extract = DBX_EXTRACT_IGNORE;
    
    if (cond < 0) {
      /* message not found on disk: extract from dbx */
      if (!ignore)
        dbx->info[imessage].extract = DBX_EXTRACT_FORCE; 
      imessage++;
    }
    else if (cond == 0) {
      /* message found on disk: extract from dbx if modified */
      dbx->info[imessage].extract = DBX_EXTRACT_MAYBE; 
      imessage++;
      ifile++;
    }
    else {
      /* file on disk not found in dbx: delete from disk */
      if (dbx->options->keep_deleted) {
        int rc = sys_move(eml_dir, eml_files[ifile], "deleted");
        if (rc != 0) 
          perror("_extract (sys_move)");
        dbx_progress_update(dbx->progress_handle, DBX_STATUS_MOVED, -1, "%s", eml_files[ifile]);
      }
      else {
        int rc = sys_delete(eml_dir, eml_files[ifile]);
        if (rc != 0) 
          perror("_extract (sys_delete)");
        dbx_progress_update(dbx->progress_handle, DBX_STATUS_DELETED, -1, "%s", eml_files[ifile]);        
      }
      ifile++;
      (*deleted)++;
    }

    no_more_messages = (imessage == dbx->message_count);
    no_more_files = (ifile == num_eml_files);
  }

  /* sort entries by offset: should make extraction faster in most cases */
  qsort(dbx->info, dbx->message_count, sizeof(dbx_info_t), (dbx_cmpfunc_t) _dbx_offset_cmp);
  
  for(imessage = 0; imessage < dbx->message_count; imessage++) {
    dbx_save_status_t status = DBX_SAVE_NOOP;

    switch (dbx->info[imessage].extract) {
    case DBX_EXTRACT_IGNORE:
      break;
    case DBX_EXTRACT_FORCE:
      status = _maybe_save_message(dbx, imessage, eml_dir, 1);
      break;
    case DBX_EXTRACT_MAYBE:
      status = _maybe_save_message(dbx, imessage, eml_dir, 0);
      break;
    }
        
    if (status == DBX_SAVE_OK)
      (*saved)++;
    if (status == DBX_SAVE_ERROR)
      (*errors)++;
  }

  dbx_progress_pop(dbx->progress_handle,
                   "%d messages saved, %d skipped, %d errors, %d files %s",
                   *saved,
                   dbx->message_count - *saved - *errors,
                   *errors,
                   *deleted,
                   dbx->options->keep_deleted? "moved":"deleted");
  
  sys_glob_free(eml_files);
}

static int _undbx(char *dbx_dir, char *out_dir, char *dbx_file, dbx_options_t *options)
{
  int deleted = 0; 
  int saved = 0;
  int errors = 0;
  
  dbx_t *dbx = NULL;
  char *eml_dir = NULL;
  char *cwd = NULL;
  int rc = -1;

  cwd = sys_getcwd();
  if (cwd == NULL) {
    dbx_progress_message(NULL, DBX_STATUS_ERROR, "can't get current working directory");
    goto UNDBX_DONE;
  }

  rc = sys_chdir(dbx_dir);
  if (rc != 0) {
    dbx_progress_message(NULL, DBX_STATUS_ERROR, "can't chdir to %s", dbx_dir);
    goto UNDBX_DONE;
  }
  
  dbx = dbx_open(dbx_file, options);
  
  sys_chdir(cwd);

  if (dbx == NULL) {
    dbx_progress_message(NULL, DBX_STATUS_WARNING, "can't open DBX file %s", dbx_file);
    rc = -1;
    goto UNDBX_DONE;
  }

  if (!options->recover && dbx->type != DBX_TYPE_EMAIL) {
    dbx_progress_message(dbx->progress_handle, DBX_STATUS_WARNING, "DBX file %s does not contain messages", dbx_file);
    rc = -1;
    goto UNDBX_DONE;
  }

  if (!options->recover && dbx->file_size >= 0x80000000) {
    dbx_progress_message(dbx->progress_handle, DBX_STATUS_WARNING,"DBX file %s is corrupted (larger than 2GB)", dbx_file);
  }

  eml_dir = strdup(dbx_file);
  eml_dir[strlen(eml_dir) - 4] = '\0';
  rc = sys_mkdir(out_dir, eml_dir);
  if (rc != 0) {
    dbx_progress_message(dbx->progress_handle, DBX_STATUS_ERROR, "can't create directory %s/%s", out_dir, eml_dir);
    goto UNDBX_DONE;
  }

  rc = sys_chdir(out_dir);
  if (rc != 0) {
    dbx_progress_message(dbx->progress_handle, DBX_STATUS_ERROR, "can't chdir to %s", out_dir);
    goto UNDBX_DONE;
  }

  if (options->recover)
    _recover(dbx, out_dir, eml_dir, &saved, &errors);
  else
    _extract(dbx, out_dir, eml_dir, &saved, &deleted, &errors);

 UNDBX_DONE:  
  free(eml_dir);
  eml_dir = NULL;
  dbx_close(dbx);
  sys_chdir(cwd);
  free(cwd);
  cwd = NULL;

  return rc;
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


static void _usage(char *prog, int rc)
{
  FILE *stream = (rc == EXIT_SUCCESS)? stdout:stderr;
  
  fprintf(stream,
          "Usage: %s [<OPTION>] <DBX-FOLDER | DBX-FILE> [<OUTPUT-FOLDER>]\n"
          "\n"
          "Options:\n"
          "\t-h, --help        \t show this message\n"
          "\t-V, --version     \t show only version string\n"
          "\t-v, --verbosity N \t set verbosity level to N [default: 3]\n"
          "\t-r, --recover     \t enable recovery mode\n"
          "\t-s, --safe-mode   \t generate locale-safe file names\n"
          "\t-k, --keep-deleted\t do not delete extracted messages\n"
          "\t                  \t that were deleted from the dbx file\n" 
          "\t-i, --ignore0     \t ignore empty messages\n"
          "\t-d, --debug       \t output debug messages\n",
          prog);
  
  exit(rc);
}

#ifdef _WIN32
#include <windows.h>
static void _gui(char *prog)
{
  FILE *rfp = NULL;
  char fn[256];
  char cmd[256];
  snprintf(fn, 256, "%s/undbx.hta", sys_dirname(prog));
  rfp = fopen(fn, "r");
  if (rfp == NULL) {
    char msg[256];
    snprintf(msg, 256, "Error: %s not found.\nPlease extract ALL files from undbx-%s.zip, and try again.",
             fn, DBX_VERSION);
    MessageBox(NULL, msg, "UnDBX", MB_ICONERROR|MB_OK);
    exit(EXIT_FAILURE);
  }
  fclose(rfp);
  snprintf(cmd, 256, "mshta \"%s\"", fn);
  system(cmd);
  exit(EXIT_SUCCESS);
}
#endif

int main(int argc, char *argv[])
{
  int n = 0;
  int fail = 0;
  char **dbx_files = NULL;
  char *dbx_dir = NULL;
  char *out_dir = NULL;
  int num_dbx_files = 0;
  dbx_options_t options = { 0 };
  int c = -1;

  printf("UnDBX v" DBX_VERSION " (" __DATE__ ")\n");
  fflush(stdout);

  if (argc == 1) {
#ifdef _WIN32
    _gui(argv[0]);
#else
    _usage(argv[0], EXIT_SUCCESS);
#endif
  }

  options.verbosity = DBX_VERBOSITY_INFO;
  
  while (1) {
    static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'V'},
      {"verbosity", required_argument, NULL, 'v'},
      {"recover", no_argument, NULL, 'r'},
      {"safe-mode", no_argument, NULL, 's'},
      {"keep-deleted", no_argument, NULL, 'k'},
      {"ignore0", no_argument, NULL, 'i'},
      {"debug", no_argument, NULL, 'd'},
      {0, 0, 0, 0}
    };
    
    c = getopt_long(argc, argv, "hVv:rskid", long_options, NULL);
    if (c == -1 || c == '?' || c == ':')
      break;
    
    switch (c) {
    case 'h':
      _usage(argv[0], EXIT_SUCCESS);
      break;
    case 'V':
      exit(EXIT_SUCCESS);
      break;
    case 'v':
      options.verbosity = atoi(optarg);
      break;
    case 'r':
      options.recover = 1;
      break;
    case 's':
      options.safe_mode = 1;
      break;
    case 'k':
      options.keep_deleted = 1;
      break;
    case 'i':
      options.ignore0 = 1;
      break;
    case 'd':
      options.debug = 1;
      break;
    default:
      break;
    }
  }
  
  if (c == '?') 
    _usage(argv[0], EXIT_FAILURE);

  if (argc - optind < 1 || argc - optind > 2) {
    fprintf(stderr, "error: bad command line\n");
    _usage(argv[0], EXIT_FAILURE);
  }

  dbx_dir = strdup(argv[optind]);
  
  if (argc - optind == 2)
    out_dir = argv[optind + 1];
  else
    out_dir = ".";

  dbx_files = _get_files(&dbx_dir, &num_dbx_files);
  for(n = 0; n < num_dbx_files; n++) {
    if (_undbx(dbx_dir, out_dir, dbx_files[n], &options))
      fail++;
  }

  if (num_dbx_files > 0)
    dbx_progress_message(NULL, DBX_STATUS_OK, "Extracted %d out of %d DBX files", n - fail, n);
  else
    dbx_progress_message(NULL, DBX_STATUS_WARNING, "can't find DBX files in \"%s\"", dbx_dir);
  
  sys_glob_free(dbx_files);
  free(dbx_dir);

  return EXIT_SUCCESS;
}
