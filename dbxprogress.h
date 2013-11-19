/*
    UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
    Copyright (C) 2008-2013 Avi Rozen <avi.rozen@gmail.com>

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

#ifndef _DBX_PROGRESS_H_
#define _DBX_PROGRESS_H_

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum {
    DBX_STATUS_OK,
    DBX_STATUS_DELETED,
    DBX_STATUS_MOVED,
    DBX_STATUS_WARNING,
    DBX_STATUS_ERROR,
    
    DBX_STATUS_LAST
  } dbx_status_t;
  
  typedef enum {
    DBX_VERBOSITY_QUIET,
    DBX_VERBOSITY_ERROR,
    DBX_VERBOSITY_WARNING,
    DBX_VERBOSITY_INFO,
    DBX_VERBOSITY_VERBOSE,
    DBX_VERBOSITY_DEBUG,    
  } dbx_verbosity_t;

  typedef struct dbx_progress_s *dbx_progress_handle_t;

  dbx_progress_handle_t dbx_progress_new(dbx_verbosity_t level);
  void dbx_progress_delete(dbx_progress_handle_t handle);
  
  void dbx_progress_push(dbx_progress_handle_t handle,
                         dbx_verbosity_t level,
                         unsigned int n,
                         char *format, ...);
  void dbx_progress_pop(dbx_progress_handle_t handle, char *format, ...);
  
  void dbx_progress_update(dbx_progress_handle_t handle,
                           dbx_status_t status,
                           unsigned int n,
                           char *format, ...);
  void dbx_progress_message(dbx_progress_handle_t handle,
                            dbx_status_t status,
                            char *format, ...);
    
#ifdef __cplusplus
};
#endif

#endif /* _DBX_PROGRESS_H_ */

