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

#ifndef _DBX_READ_H_
#define _DBX_READ_H_

#ifdef __cplusplus
extern "C" {
#endif

#define DBX_MAX_FILENAME 128 

  typedef int (*dbx_cmpfunc_t)(const void *, const void *);

  char **sys_glob(char *parent, char *pattern, int *num_files);
  typedef unsigned long long filetime_t;

  typedef enum {
    DBX_TYPE_UNKNOWN,
    DBX_TYPE_EMAIL,
    DBX_TYPE_OE4,
    DBX_TYPE_FOLDER
  } dbx_type_t;

  typedef enum {
    DBX_MASK_INDEX     = 0x01,
    DBX_MASK_FLAGS     = 0x02,
    DBX_MASK_BODYLINES = 0x04,
    DBX_MASK_MSGADDR   = 0x08,
    DBX_MASK_MSGPRIO   = 0x10,
    DBX_MASK_MSGSIZE    = 0x20
  } dbx_mask_t;

  typedef struct dbx_info_s {
    int index;
    char *filename;
    dbx_mask_t valid;
    unsigned int message_index;
    unsigned int flags;
    filetime_t send_create_time;
    unsigned int body_lines;
    unsigned int message_address;
    char *original_subject;
    filetime_t save_time;
    char *message_id;
    char *subject;
    char *sender_address_and_name;
    char *message_id_replied_to;
    char *server_newsgroup_message_number;
    char *server;
    char *sender_name;
    char *sender_address;
    unsigned int message_priority;
    unsigned int message_size;
    filetime_t receive_create_time;
    char *receiver_name;
    char *receiver_address;
    char *account_name;
    char *account_registry_key;
  } dbx_info_t;

  typedef struct dbx_s {
    FILE *file;
    unsigned long long file_size;
    dbx_type_t type;
    int message_count;
    int capacity;
    dbx_info_t *info;  
  } dbx_t;

  extern dbx_t *dbx_open(char *filename);
  extern void dbx_close(dbx_t *dbx);
  extern char *dbx_message(dbx_t *dbx, int msg_number, unsigned int *psize);
  
#ifdef __cplusplus
};
#endif

#endif /* _DBX_READ_H_ */
