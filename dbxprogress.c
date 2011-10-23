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
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "dbxprogress.h"


typedef struct dbx_progress_bar_s {
  int enabled;
  int verbose;
  float max;
  float last;
  float last_percent;
  float delta;
} dbx_progress_bar_t;


typedef struct dbx_progress_s {
  dbx_verbosity_t verbosity;
  dbx_progress_bar_t *bars;
  int count;
} dbx_progress_t;


static const char *_dbx_status_label[DBX_STATUS_LAST + 1] = {
  "OK",
  "DELETED",
  "MOVED",
  "WARNING",
  "ERROR",
  "???"
};


static void _dbx_progress_vprintf(dbx_status_t status,
                                  char *prefix,
                                  char *format,
                                  va_list ap,
                                  char *suffix)
{
  FILE *stream = (status < DBX_STATUS_WARNING)? stdout:stderr;
  fputs(prefix, stream);
  if (format)
    vfprintf(stream, format, ap);
  fputs(suffix, stream);
  fflush(stream);
}


static void _dbx_progress_printf(dbx_status_t status, char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  _dbx_progress_vprintf(status, "", format, ap, "");
  va_end(ap);
}


static const char *_dbx_status_string(dbx_status_t status)
{
  if (status < DBX_STATUS_OK || status >= DBX_STATUS_LAST)
    status = DBX_STATUS_LAST;
  return _dbx_status_label[status];
}

static void _dbx_progress_update(dbx_status_t status,
                                 dbx_progress_bar_t *bar,
                                 unsigned int n,
                                 char *format,
                                 va_list ap)
{
  if (format == NULL || !bar->verbose) {
    if (n + 1 != 0) {
      if ((n + 1) < bar->max && n - bar->last < bar->delta)
        return;
      bar->last = n;
      _dbx_progress_printf(DBX_STATUS_OK, "\b\b\b\b\b\b%5.1f%%", (n + 1) / bar->max * 100.0);
      fflush(stdout);
    }
  }
  else {
    if (n + 1 != 0) 
      _dbx_progress_printf(DBX_STATUS_OK, "\n%5.1f%% ", (n + 1) / bar->max * 100.0);
    else
      _dbx_progress_printf(DBX_STATUS_OK, "\n       ");
    _dbx_progress_printf(DBX_STATUS_OK, "[%-7s] ", _dbx_status_string(status));
    _dbx_progress_vprintf(DBX_STATUS_OK, "", format, ap, "");
  }
}

dbx_progress_handle_t dbx_progress_new(dbx_verbosity_t level)
{
  dbx_progress_handle_t handle = (dbx_progress_handle_t) calloc(1, sizeof(dbx_progress_t));
  if (handle) 
    handle->verbosity = level;
  else 
    perror("dbx_progress_new (calloc)");
  return handle;
}


void dbx_progress_delete(dbx_progress_handle_t handle)
{
  if (handle) 
    free(handle);
}
  

void dbx_progress_push(dbx_progress_handle_t handle,
                       dbx_verbosity_t level,
                       unsigned int n,
                       char *format, ...)
{
  dbx_progress_bar_t *bar = NULL;
  dbx_progress_bar_t *bars = NULL;

  if (handle == NULL)
    return;
  
  bars = (dbx_progress_bar_t *)realloc(handle->bars,
                                       (handle->count + 1) * sizeof(dbx_progress_bar_t));
  if (bars == NULL) {
    perror("dbx_progress_push (realloc)");
    return;
  }
  
  handle->bars = bars;
  bar = handle->bars + handle->count;
  handle->count++;

  bar->enabled = (level <= handle->verbosity);
  bar->verbose = (level <  handle->verbosity);
  bar->last = 0;
  bar->max = (float) n;
  bar->last_percent = 0;
  bar->delta = bar->max / 1000.0;

  if (bar->enabled) {
    va_list ap;
    va_start(ap, format);
    _dbx_progress_vprintf(DBX_STATUS_OK, "", format, ap, ":       ");
    va_end(ap);
  }
}


void dbx_progress_pop(dbx_progress_handle_t handle, char *format, ...)
{
  dbx_progress_bar_t *bar = handle->bars + handle->count - 1;
  dbx_progress_bar_t *bars = NULL;

  if (handle == NULL || handle->count == 0)
    return;
  
  if (bar->enabled) {
    va_list ap;
    va_start(ap, format);
    _dbx_progress_vprintf(DBX_STATUS_OK, "\n", format, ap, format? "\n":"");
    va_end(ap);
  }

  if (handle->count == 1) {
    free(handle->bars);
  }
  else {
    bars = (dbx_progress_bar_t *)realloc(handle->bars,
                                         (handle->count - 1) * sizeof(dbx_progress_bar_t));
    if (bars == NULL) {
      perror("dbx_progress_pop (realloc)");
      return;
    }
  }
  
  handle->bars = bars;
  handle->count--;
}
  

void dbx_progress_update(dbx_progress_handle_t handle,
                         dbx_status_t status,
                         unsigned int n,
                         char *format, ...)
{
  dbx_progress_bar_t *bar = NULL;

  if (handle == NULL || handle->count == 0)
    return;
  
  bar = handle->bars + handle->count - 1;

  if (bar->enabled) {
    va_list ap;
    va_start(ap, format);
    _dbx_progress_update(status, bar, n, format, ap);
    va_end(ap);
  }
}


void dbx_progress_message(dbx_progress_handle_t handle,
                          dbx_status_t status,
                          char *format, ...)
{
  dbx_progress_bar_t *bar = NULL;

  if (handle && handle->count > 0)
    bar = handle->bars + handle->count - 1;

  if (bar == NULL || bar->enabled) {
    va_list ap;
    va_start(ap, format);
    _dbx_progress_vprintf(status, "", format, ap, "\n");
    va_end(ap);
  }
}
