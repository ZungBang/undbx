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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "dbxread.h"
#include "emlread.h"

#define INDEX_POINTER 0xE4
#define ITEM_COUNT    0xC4

static int _dbx_info_cmp(const dbx_info_t *ia, const dbx_info_t *ib)
{
  int res = strcmp(ia->filename, ib->filename);
  if (res == 0) {
    res = ia->index - ib->index;
  }
  return res;
}

static char *_dbx_read_string(FILE *file, int offset)
{
  char c[256] = {};
  char *s = NULL;
  int n = 0;
  int l = 0;

  fseek(file, offset, SEEK_SET);

  do {
    sys_fread(c, 1, 255, file);
    l = strlen(c);
    s = realloc(s, n + l + 1);
    memcpy(s + n, c, l);
    n += l;
    s[n] = '\0';
  } while (l == 255);

  return s;
}

static filetime_t _dbx_read_date(FILE *file, int offset)
{
  filetime_t filetime = 0;
  fseek(file, offset, SEEK_SET);
  sys_fread_long_long((long long int *)&filetime, file);
  return filetime;
}

static int _dbx_read_int(FILE *file, int offset, int value)
{
  int val = value;
  if (offset) {
    fseek(file, offset, SEEK_SET);
    sys_fread_int(&val, file);
  }
  return val;
}

static int _dbx_read_msg_offset(dbx_t *dbx, int msg_number)
{
  int size = 0;
  int index = 0;
  int value = 0;
  unsigned char type = 0;
  int msg_offset = 0;
  int msg_offset_ptr = 0;
  char count = 0;
  int i = 0;

  index = dbx->info[msg_number].index;

  fseek(dbx->file, index + 4, SEEK_SET);
  sys_fread_int(&size, dbx->file);
  fseek(dbx->file, 2, SEEK_CUR);
  sys_fread(&count, 1, 1, dbx->file);
  fseek(dbx->file, 1, SEEK_CUR);

  for (i = 0; i < count; i++) {
    sys_fread_int((int *)&value, dbx->file);
    type = value & 0xFF;
    value = (value >> 8) & 0xFFFFFF;                

    if (type == 0x84) {
      msg_offset = value;
      break;
    }
    if (type == 0x04) {
      msg_offset_ptr = index + 12 + value + 4 * count;
      break;
    }
  }
        
  if (msg_offset == 0 && msg_offset_ptr != 0) {
    fseek(dbx->file, msg_offset_ptr, SEEK_SET);
    sys_fread_int(&msg_offset, dbx->file);
  }

  return msg_offset;
}


static void _dbx_sanitize_filename(char *filename)
{
  char *c = NULL;
  static const char * const invalid_characters = "\\/?\"<>*|:";
  static const char valid_char = '_';

  if (!filename)
    return;

  for (c = filename; *c; c++)
    if ((unsigned char)*c < 32 || strchr(invalid_characters, *c))
      *c = valid_char;
}

static void _dbx_set_filename(dbx_info_t *info)
{
  char filename[DBX_MAX_FILENAME];
  char suffix[sizeof(".00000000.00000000.eml.00000000")];

  snprintf(filename, DBX_MAX_FILENAME - sizeof(suffix), "%.15s_%.15s_%.15s_%.15s_%s",
           info->sender_name? info->sender_name:"_(no_name)_",
           info->sender_address? info->sender_address:"_(no_address)_",
           info->receiver_name? info->receiver_name:"_(no_name)_",
           info->receiver_address? info->receiver_address:"_(no_address)_",
           info->subject? info->subject:"(no_subject)");

  sprintf(suffix, ".%08X.%08X.eml.00000000",
          (unsigned int) (info->receive_create_time & 0xFFFFFFFFULL),
          (unsigned int) (info->send_create_time & 0xFFFFFFFFULL));
  strcat(filename, suffix);
  
  _dbx_sanitize_filename(filename);

  info->filename = strdup(filename);
  /* remove trailing extra space (reserved for uniquification of filename) */
  info->filename[strlen(info->filename) - sizeof("00000000")] = '\0';
}

static void _dbx_uniquify_filenames(dbx_t *dbx)
{
  int i = 1;
  static const int cl = sizeof(".eml") - 1;

  while (i < dbx->message_count) {
    int n;

    /* skip info entries with non-equal filenames */
    while (i < dbx->message_count && strcmp(dbx->info[i - 1].filename, dbx->info[i].filename) != 0)
      i++;

    /* uniquify equal filename entries */
    n = 1;
    if (i < dbx->message_count) {
      do {
        sprintf(dbx->info[i - 1].filename + strlen(dbx->info[i - 1].filename) - cl,
                ".%08X.eml",
                (unsigned int) dbx->info[i - 1].index);
        i++;
        n++;
      } while (i < dbx->message_count && strcmp(dbx->info[i - 1].filename, dbx->info[i].filename) == 0);

      sprintf(dbx->info[i - 1].filename + strlen(dbx->info[i - 1].filename) - cl,
              ".%08X.eml",
              (unsigned int) dbx->info[i - 1].index);
    }
  }
}

static void _dbx_read_info(dbx_t *dbx)
{
  int i;

  for(i = 0; i < dbx->message_count; i++) {
    int j;
    int size;
    int count = 0;
    int index = dbx->info[i].index;
    int offset = 0;
    int pos = index + 12;

    fseek(dbx->file, index + 4, SEEK_SET);
    sys_fread_int(&size, dbx->file);
    sys_fread_int(&count, dbx->file);
    count = (count & 0x00FF0000) >> 16;

    dbx->info[i].valid = 0;

    for (j = 0; j < count; j++) {
      int type = 0;
      unsigned int value = 0;

      fseek(dbx->file, pos, SEEK_SET);
      sys_fread_int((int*)&value, dbx->file);
      type = value & 0xFF;
      value = (value >> 8) & 0xFFFFFF;

      /* msb means direct storage */
      offset = (type & 0x80)? 0:(index + 12 + 4 * count + value);

      /* dirt ugly code follows ... */
      switch (type & 0x7f) {
      case 0x00:
        dbx->info[i].message_index = _dbx_read_int(dbx->file, offset, value);
        dbx->info[i].valid |= DBX_MASK_INDEX;
        break;
      case 0x01:
        dbx->info[i].flags = _dbx_read_int(dbx->file, offset, value);
        dbx->info[i].valid |= DBX_MASK_FLAGS;
        break;
      case 0x02:
        dbx->info[i].send_create_time = _dbx_read_date(dbx->file, offset);
        break;
      case 0x03:
        dbx->info[i].body_lines = _dbx_read_int(dbx->file, offset, value);
        dbx->info[i].valid |= DBX_MASK_BODYLINES;
        break;
      case 0x04:
        dbx->info[i].message_address = _dbx_read_int(dbx->file, offset, value);
        dbx->info[i].valid |= DBX_MASK_MSGADDR;
        break;
      case 0x05:
        dbx->info[i].original_subject = _dbx_read_string(dbx->file, offset);
        break;
      case 0x06:
        dbx->info[i].save_time = _dbx_read_date(dbx->file, offset);
        break;
      case 0x07:
        dbx->info[i].message_id = _dbx_read_string(dbx->file, offset);
        break;
      case 0x08:
        dbx->info[i].subject = _dbx_read_string(dbx->file, offset);
        break;
      case 0x09:
        dbx->info[i].sender_address_and_name = _dbx_read_string(dbx->file, offset);
        break;
      case 0x0A:
        dbx->info[i].message_id_replied_to = _dbx_read_string(dbx->file, offset);
        break;
      case 0x0B:
        dbx->info[i].server_newsgroup_message_number = _dbx_read_string(dbx->file, offset);
        break;
      case 0x0C:
        dbx->info[i].server = _dbx_read_string(dbx->file, offset);
        break;
      case 0x0D:
        dbx->info[i].sender_name = _dbx_read_string(dbx->file, offset);
        break;
      case 0x0E:
        dbx->info[i].sender_address = _dbx_read_string(dbx->file, offset);
        break;
      case 0x10:
        dbx->info[i].message_priority = _dbx_read_int(dbx->file, offset, value);
        dbx->info[i].valid |= DBX_MASK_MSGPRIO;
        break;
      case 0x11:
        dbx->info[i].message_size = _dbx_read_int(dbx->file, offset, value);
        dbx->info[i].valid |= DBX_MASK_MSGSIZE;
        break;
      case 0x12:
        dbx->info[i].receive_create_time = _dbx_read_date(dbx->file, offset);
        break;
      case 0x13:
        dbx->info[i].receiver_name = _dbx_read_string(dbx->file, offset);
        break;
      case 0x14:
        dbx->info[i].receiver_address = _dbx_read_string(dbx->file, offset);
        break;
      case 0x1A:
        dbx->info[i].account_name = _dbx_read_string(dbx->file, offset);
        break;
      case 0x1B:
        dbx->info[i].account_registry_key = _dbx_read_string(dbx->file, offset);
        break;
      }
      pos += 4;
    }

    dbx->info[i].offset = _dbx_read_msg_offset(dbx, i);
    
    if (dbx->options->safe_mode) {
      char filename[DBX_MAX_FILENAME];
      int msg_offset = dbx->info[i].offset;
      if (dbx->info[i].offset == 0)  /* message only in index, not downloaded yet */
        msg_offset = dbx->info[i].index;
      sprintf(filename, "%08X.eml", (unsigned int) msg_offset);
      dbx->info[i].filename = strdup(filename);
    }
    else {
      _dbx_set_filename(dbx->info + i);
    }
  }
}


static int _dbx_read_index(dbx_t *dbx, int pos)
{
  int i;
  int next_table;
  char ptr_count = 0;
  int index_count;

  if (pos <= 0 || dbx->file_size <= (unsigned long long int)pos) {
    dbx_progress_message(dbx->progress_handle,
                         DBX_STATUS_WARNING,
                         "DBX file %s is corrupted (bad seek offset %08X)",
                         dbx->filename,
                         pos);
    return 0;
  }

  fseek(dbx->file, pos + 8, SEEK_SET);
  sys_fread_int(&next_table, dbx->file);
  fseek(dbx->file, 5, SEEK_CUR);
  sys_fread(&ptr_count, 1, 1, dbx->file);
  if (ptr_count <= 0) {
    dbx_progress_message(dbx->progress_handle,
                         DBX_STATUS_WARNING,
                         "DBX file %s is corrupted (bad count %d at offset %08X)",
                         dbx->filename,
                         ptr_count,
                         pos + 8 + 4 + 5);
    return 0;
  }
  fseek(dbx->file, 2, SEEK_CUR);
  sys_fread_int(&index_count, dbx->file);

  if (index_count > 0) {
    if (!_dbx_read_index(dbx, next_table))
      return 0;
  }

  pos += 24;

  dbx->info = (dbx_info_t *)realloc(dbx->info, (dbx->capacity + ptr_count) * sizeof(dbx_info_t));
  dbx->capacity += ptr_count;
  for (i = 0; i < ptr_count; i++) {
    int index_ptr;
    fseek(dbx->file, pos, SEEK_SET);
    sys_fread_int(&index_ptr, dbx->file);
    sys_fread_int(&next_table, dbx->file);
    sys_fread_int(&index_count, dbx->file);

    memset(dbx->info + dbx->message_count, 0, sizeof(dbx_info_t));
    dbx->info[dbx->message_count].index = index_ptr;
    dbx->message_count++;

    pos += 12;
    if (index_count > 0) {
      if (!_dbx_read_index(dbx, next_table))
        return 0;
    }
  }

  return 1;
}


static int _dbx_read_indexes(dbx_t *dbx)
{
  int index_ptr;
  int item_count;

  fseek(dbx->file, INDEX_POINTER, SEEK_SET);
  sys_fread_int(&index_ptr, dbx->file);

  fseek(dbx->file, ITEM_COUNT, SEEK_SET);
  sys_fread_int(&item_count, dbx->file);

  if (item_count > 0)
    return _dbx_read_index(dbx, index_ptr);
  else
    return 0;
}

static dbx_chains_t *_dbx_get_scan_chains(dbx_t *dbx, long long int offset, int deleted)
{
  int i = 0;
  dbx_chains_t *scan = NULL;
  for (i = 0; i < dbx->scan_count; i++) {
    if (dbx->scan[i].offset == offset &&
        dbx->scan[i].deleted == deleted) {
      return dbx->scan + i;
    }
  }
  dbx->scan_count++;
  dbx->scan = (dbx_chains_t *)realloc(dbx->scan, sizeof(dbx_chains_t) * dbx->scan_count);
  scan = dbx->scan + dbx->scan_count - 1;
  scan->offset = offset;
  scan->deleted = deleted;
  scan->fragment_count = 0;
  scan->fragments = NULL;
  scan->count = 0;
  scan->chains = NULL;
  return scan;
}

static void _dbx_scan(dbx_t *dbx)
{
  int header[8] = {0};
  int header_start = 0;
  int ready = 0;
  unsigned long long int i = 0;
  int j = 0;
  
  dbx_progress_push(dbx->progress_handle, DBX_VERBOSITY_INFO, dbx->file_size, "Scanning %s", dbx->filename);

  for (i = 0; i < dbx->file_size; i += 4) {
    int deleted = 0;
    dbx_chains_t *chains = NULL;
    dbx_fragment_t *other = NULL;
    dbx_fragment_t *fragment = NULL;

    dbx_progress_update(dbx->progress_handle, DBX_STATUS_OK, i, NULL);

    /* initialize header buffer */
    if (!ready) {
      header_start = 0;
      for (j = 0; j < 4; j++)
        sys_fread_int(header + j, dbx->file);
      i += 16;
      ready = 1;
    }

    sys_fread_int(header + ((header_start + 4) & 7), dbx->file);

    /* message fragment header signature:
       =================================
       1st word value equals offset into file 
       2nd word is 0x200
       3rd word is fragment length: must be positive, but not more than 0x200 
       4th word value is the file offset of the next fragment, or 0 if last

       deleted message fragment header signature:
       =========================================
       1st word value equals offset into file
       2nd word is 0x1FC
       3rd word is 0x210
       4th word value is offset of next fragment or 0 if last
       5th word value is offset of previous fragment (clobbering message data!)

       sanity: we check that all offsets are less than file size,
       and are a multiple of 4 and that fragment does not point to itself
    */

    int header_offset_diff = header[header_start] - i;
    int message_fragment_found = (header[(header_start + 1) & 7] == 0x200 &&
                                  header[(header_start + 2) & 7] > 0 &&
                                  header[(header_start + 2) & 7] <= 0x200 &&
                                  header[(header_start + 3) & 7] >= 0 &&
                                  header[(header_start + 3) & 7] < dbx->file_size &&
                                  (header[(header_start + 3) & 7] & 3) == 0 &&
                                  header[(header_start + 3) & 7] != header[header_start])? 1:0;
    int deleted_fragment_found = (header[(header_start + 1) & 7] == 0x1FC &&
                                  header[(header_start + 2) & 7] == 0x210 &&
                                  header[(header_start + 3) & 7] < dbx->file_size &&
                                  (header[(header_start + 3) & 7] & 3) == 0 &&
                                  header[(header_start + 3) & 7] != header[header_start] &&
                                  header[(header_start + 4) & 7] >= 0 &&
                                  header[(header_start + 4) & 7] < dbx->file_size &&
                                  (header[(header_start + 4) & 7] & 3) == 0)? 1:0;
    
    if (!message_fragment_found &&
        !deleted_fragment_found) {
      header_start = (header_start + 1) & 7;
      continue;
    }

    if (dbx->options->debug) {
      int iii;
      for (iii = 0; iii < 5; iii++) {
        printf("%08X ", header[(header_start + iii) & 7]);
      }
      printf("\n");
    }
    
    /* add fragment to fragment list */
    deleted = (header[(header_start + 1) & 7] == 0x1FC)? 1:0;
    chains = _dbx_get_scan_chains(dbx, header_offset_diff, deleted);

    if ((chains->fragment_count % 4096) == 0)
      chains->fragments = (dbx_fragment_t *)realloc(chains->fragments,
                                                    sizeof(dbx_fragment_t) * (chains->fragment_count + 4096));


    fragment = chains->fragments + chains->fragment_count;
    fragment->prev = -1;
    fragment->next = -1;
    fragment->offset = header[header_start];
    fragment->offset_next = header[(header_start + 3) & 7];
    fragment->offset_prev = header[(header_start + 4) & 7]; /* only valid for deleted messages */
    fragment->size = header[(header_start + 2) & 7];
    chains->fragment_count++;
    chains->count++;

    /* check if previous fragment is next fragment, if we already passed it */
    other = fragment - 1;
    if (other >= chains->fragments) {
      if (fragment->offset_next &&
          fragment->offset_next < fragment->offset &&
          other->prev < 0 && /* avoid already used fragments */
          fragment->offset_next == other->offset &&
          (!deleted || other->offset_prev == fragment->offset)) {
        fragment->next = other - chains->fragments;
        other->prev = chains->fragment_count - 1;
        chains->count--;
      }
      else if (other->next < 0 &&
               fragment->offset == other->offset_next &&
               (!deleted || fragment->offset_prev == other->offset)) {
        fragment->prev = other - chains->fragments;
        other->next = chains->fragment_count - 1;
        chains->count--;
      }
    }

    /* skip contents of fragment */
    i += 0x200 - 4;
    fseek(dbx->file, i + 20, SEEK_SET);
    /* header buffer should be refilled */
    ready = 0;
  }

  for (j = 0; j < dbx->scan_count; j++) {
    if (dbx->scan[j].count) {
      dbx_chains_t *chains = NULL;
      dbx_fragment_t *other = NULL;
      dbx_fragment_t *fragment = NULL;
      unsigned int k0, k1, k2;

      chains = &dbx->scan[j];
      for (i = 0; i < chains->fragment_count; i++) {
        fragment = chains->fragments + i;
        if (fragment->offset_next == 0 || fragment->next >= 0)
          continue;

        k0 = 0;
        k2 = chains->fragment_count;
        for (k1 = i + (i < k2)? 1:0;  k0 < k2;  k1 = (k0 + k2) / 2) {
          other = chains->fragments + k1;
          if (other->offset < fragment->offset_next) {
            if (k1 == k0)
              break;
            k0 = k1;
            continue;
          }
          if (other->offset > fragment->offset_next) {
            k2 = k1;
            continue;
          }
          if (other->prev >= 0)
            break;
          if (dbx->scan[j].deleted && other->offset_prev != fragment->offset)
            break;
          fragment->next = k1;
          other->prev = i;
          chains->count--;
          break;
        }
      }
    }
  }

  /* collect the fragments that start messages chains
     messages start with fragments where prev == -1,
     i.e. no other fragment points to it
  */
  for (j = 0; j < dbx->scan_count; j++) {
    if (dbx->scan[j].count) {
      int nm = 0;
      int nf = 0;
      int cnf = 0;
      dbx_fragment_t *pf = NULL;

      dbx->scan[j].chains = (dbx_fragment_t **)calloc(dbx->scan[j].count, sizeof(dbx_fragment_t *));
      dbx->scan[j].chain_fragment_count = (int *)calloc(dbx->scan[j].count, sizeof(int));
      for (nm = 0; nm < dbx->scan[j].count; nm++) {
        while (dbx->scan[j].fragments[nf].prev >= 0)
          nf++;
        dbx->scan[j].chains[nm] = dbx->scan[j].fragments + nf;
        /* count fragments in chain */
        for (cnf = nf, pf = dbx->scan[j].chains[nm]; cnf >= 0; cnf = pf->next) {
          dbx->scan[j].chain_fragment_count[nm]++;
          pf = dbx->scan[j].fragments + cnf;
        }
        nf++;
      }
    }
  }

  dbx_progress_update(dbx->progress_handle, DBX_STATUS_OK, dbx->file_size, NULL);
  dbx_progress_pop(dbx->progress_handle, NULL);
}

static void _dbx_init(dbx_t *dbx)
{
  int i = 0;
  unsigned int signature[4];

  dbx->message_count = 0;
  dbx->capacity = 0;
  dbx->info = NULL;

  for(i = 0; i < 4; i++)
    sys_fread_int((int *)(signature + i), dbx->file);

  if (signature[0] == 0xFE12ADCF &&
      signature[1] == 0x6F74FDC5 &&
      signature[2] == 0x11D1E366 &&
      signature[3] == 0xC0004E9A) {
    dbx->type = DBX_TYPE_EMAIL;
  }
  else if (signature[0] == 0x36464D4A &&
           signature[1] == 0x00010003) {
    dbx->type = DBX_TYPE_OE4;
  }
  else if (signature[0]==0xFE12ADCF &&
           signature[1]==0x6F74FDC6 &&
           signature[2]==0x11D1E366 &&
           signature[3]==0xC0004E9A) {
    dbx->type = DBX_TYPE_FOLDER;
  }
  else {
    dbx->type = DBX_TYPE_UNKNOWN;
  }

  if (dbx->options->recover) {
    /* we ignore file type in recovery mode */
    _dbx_scan(dbx);
  }
  else if (dbx->type == DBX_TYPE_EMAIL) {
    _dbx_read_indexes(dbx);
    _dbx_read_info(dbx);
    qsort(dbx->info, dbx->message_count, sizeof(dbx_info_t), (dbx_cmpfunc_t) _dbx_info_cmp);
    if (!dbx->options->safe_mode) /* filenames should already be unique in safe mode */
      _dbx_uniquify_filenames(dbx);
  }
}


dbx_t *dbx_open(char *filename, dbx_options_t *options)
{
  dbx_t *dbx = (dbx_t *) calloc(1, sizeof(dbx_t));

  if (dbx) {
    dbx->progress_handle = dbx_progress_new(options->verbosity);
    dbx->file = fopen(filename, "rb");
    if (dbx->file == NULL) {
      free(dbx);
      dbx = NULL;
    }
    else {
      struct stat st;
      if (stat(filename, &st) != 0) {
        perror("dbx_open");
        dbx = NULL;
      }
      else {
        dbx->filename = strdup(filename);
        dbx->file_size = sys_filesize(".", filename);
        dbx->options = options;
        _dbx_init(dbx);
      }
    }
  }

  return dbx;
}

void dbx_close(dbx_t *dbx)
{
  int i;

  if (dbx) {
    if (dbx->file) {
      fclose(dbx->file);
      dbx->file = NULL;
    }

    for(i = 0; i < dbx->message_count; i++) {
      free(dbx->info[i].filename);
      free(dbx->info[i].original_subject);
      free(dbx->info[i].message_id);
      free(dbx->info[i].subject);
      free(dbx->info[i].sender_address_and_name);
      free(dbx->info[i].message_id_replied_to);
      free(dbx->info[i].server_newsgroup_message_number);
      free(dbx->info[i].server);
      free(dbx->info[i].sender_name);
      free(dbx->info[i].sender_address);
      free(dbx->info[i].receiver_name);
      free(dbx->info[i].receiver_address);
      free(dbx->info[i].account_name);
      free(dbx->info[i].account_registry_key);
    }

    free(dbx->info);
    dbx->info = NULL;
    free(dbx->filename);

    for (i = 0; i < dbx->scan_count; i++) {
      if (dbx->scan[i].chains) {
        free(dbx->scan[i].chains);
        dbx->scan[i].chains = NULL;
        dbx->scan[i].count = 0;
      }
      if (dbx->scan[i].chain_fragment_count) {
        free(dbx->scan[i].chain_fragment_count);
        dbx->scan[i].chain_fragment_count = NULL;
      }
      if (dbx->scan[i].fragments) {
        free(dbx->scan[i].fragments);
        dbx->scan[i].fragments = NULL;
        dbx->scan[i].fragment_count = 0;
      }
    }
    if (dbx->scan) {
      free(dbx->scan);
      dbx->scan = NULL;
      dbx->scan_count = 0;
    }

    dbx_progress_delete(dbx->progress_handle);
    free(dbx);
  }
}

char *dbx_message(dbx_t *dbx, int msg_number, unsigned int *psize)
{
  unsigned int total_size = 0;
  short block_size = 0;
  int i = 0;
  char *buf = NULL;

  if (psize)
    *psize = 0;

  if (dbx == NULL || msg_number >= dbx->message_count)
    return NULL;

  i = dbx->info[msg_number].offset;
  total_size = 0;

  while (i != 0) {
    fseek(dbx->file, i + 8, SEEK_SET);
    block_size=0;
    sys_fread_short(&block_size, dbx->file);
    if (block_size <= 0 || block_size > 0x200) {
      dbx_progress_message(dbx->progress_handle,
                           DBX_STATUS_WARNING,
                           "DBX file %s is corrupted (bad block size %04X at offset %08X)",
                           dbx->filename,
                           block_size,
                           i + 8);
      break;
    }
    fseek(dbx->file, 2, SEEK_CUR);
    sys_fread_int(&i, dbx->file);
    total_size += block_size;
    buf = realloc(buf, total_size + 1);
    sys_fread(buf + total_size - block_size, block_size, 1, dbx->file);
  }

  if (buf)
    buf[total_size]='\0';

  if (psize)
    *psize = total_size;

  return buf;
}

char *dbx_recover_message(dbx_t *dbx, int chain_index, int msg_number, unsigned int *psize, time_t *ptimestamp, char **pfilename)
{
  unsigned int size = 0;
  char filename[DBX_MAX_FILENAME];
  char suffix[sizeof(".0000000000000000.eml")];
  char *message = NULL;
  dbx_fragment_t *pfragment = NULL;
  int ifragment = dbx->scan[chain_index].chains[msg_number] - dbx->scan[chain_index].fragments;
  unsigned int fsize = 0;

  time_t timestamp = 0;
  char *subject = NULL;
  char *to = NULL;
  char *from = NULL;

  if (dbx->scan[chain_index].chain_fragment_count[msg_number] > 0)
    message = (char *)calloc(1, dbx->scan[chain_index].chain_fragment_count[msg_number] * 0x200 + 1);
  if (message == NULL)
    return message;
  
  for ( ; ifragment >= 0; ifragment = pfragment->next) {
    pfragment = dbx->scan[chain_index].fragments + ifragment;
    /* deleted fragments have size 0x210, which is wrong - it's 0x200 */
    fsize = pfragment->size <= 0x200? pfragment->size : 0x200;
    fseek(dbx->file, pfragment->offset + 16 - dbx->scan[chain_index].offset, SEEK_SET);
    sys_fread(message + size, fsize, 1, dbx->file);
    /* each deleted fragment starts with bad 4 bytes
       (it's set to the offset of the previous fragment)
       so we replace them with 4 dashes, which eases
       eml parsing and should at least make the text readable
    */
    if (dbx->scan[chain_index].deleted)
      memset(message + size, '-', 4);
    if (dbx->options->debug) 
      printf("%08X: %08X %08X %04X\n",
             size,
             pfragment->offset,
             pfragment->offset_next,
             fsize);
    size += fsize;
  }

  if (message) {
    message[size] = '\0';
    /* lose trailing nul characters in deleted messages,
       since size of last fragment is unknown
    */
    if (dbx->scan[chain_index].deleted) {
      int zeros = 0;
      while (zeros <= size && message[size - zeros] == 0)
        zeros++;
      size -= zeros - 1;
    }

    eml_parse(message, &subject, &from, &to, &timestamp);
  }

  unsigned long long int message_offset =
    dbx->scan[chain_index].chains[msg_number]->offset - dbx->scan[chain_index].offset;
  if (dbx->options->safe_mode) {
    sprintf(filename,
            "%016"
#ifndef WIN32
            "ll"
#else
            "I64"
#endif
            "X.eml", message_offset);
  }
  else {
    snprintf(filename, DBX_MAX_FILENAME - sizeof(suffix), "%.31s_%.31s_%s",
             from? (from[0]=='"'? from+1:from):"_(no_sender)",
             to? (to[0]=='"'? to+1:to):"(no_receiver)",
             subject? subject:"(no_subject)");
    
    sprintf(suffix,
            ".%016"
#ifndef WIN32
            "ll"
#else
            "I64"
#endif
            "X.eml", message_offset);
    strcat(filename, suffix);
    
    _dbx_sanitize_filename(filename);
  }

  if (message) {
    free(subject);
    free(to);
    free(from);
  }
  
  *pfilename = strdup(filename);
  *ptimestamp = timestamp;
  *psize = size;

  return message;
}


