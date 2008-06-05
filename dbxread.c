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
#include "dbxread.h"

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

static char *_dbx_read_string(FILE *file, long offset)
{
  char c[256];
  char *s = NULL;
  int n = 0;
  int l = 0;
  
  fseek(file, offset, SEEK_SET);
  c[255]='\0';

  do {
    fread(c, 1, 255, file);
    l = strlen(c);
    s = realloc(s, n + l + 1);
    strncpy(s + n, c, l);
    n += l;
    s[n] = '\0';
  } while (l == 255);

  return s;
}

static filetime_t _dbx_read_date(FILE *file, long offset)
{
  filetime_t filetime = 0;
  fseek(file, offset, SEEK_SET);
  fread(&filetime, 8, 1, file);
  return filetime;
}

static long _dbx_read_long(FILE *file, long offset, long value)
{
  long val = value;
  if (offset) {
    fseek(file, offset, SEEK_SET);
    fread(&val, 4, 1, file);
  }
  return val;
}

static void _dbx_set_filename(dbx_info_t *info)
{
  int i;
  int l;
  char filename[DBX_MAX_FILENAME];
  char suffix[sizeof(".00000000.00000000.eml.00000000")];

  static const char * const invalid_characters = "\\/?\"<>*|:";
  static const char valid_char = '_';

  snprintf(filename, DBX_MAX_FILENAME - sizeof(suffix) - 1, "%s_%s_%s",
	   info->sender_name? info->sender_name:"_(no_name)_",
	   info->sender_address? info->sender_address:"_(no_address)_",
	   info->subject? info->subject:"(no_subject)");
  
  sprintf(suffix, ".%08X.%08X.eml.00000000",
	  (unsigned int) (info->receive_create_time & 0xFFFFFFFFULL),
	  (unsigned int) (info->send_create_time & 0xFFFFFFFFULL));
  strcat(filename, suffix);

  l = strlen(filename);
  for (i = 0; i < l; i++) {
    if ((unsigned char)filename[i] < 32 || strchr(invalid_characters, filename[i])) {
      filename[i] = valid_char;
    }
  }
  
  info->filename = strdup(filename);
  /* remove trailing extra space (reserved for uniquification of filename) */
  info->filename[l - sizeof("00000000")] = '\0';
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
    long size;
    int count = 0;
    long index = dbx->info[i].index;
    long offset = 0;
    long pos = index + 12L;

    fseek(dbx->file, index + 4L, SEEK_SET);
    fread(&size, 4, 1, dbx->file);
    fseek(dbx->file, 2L, SEEK_CUR);
    fread(&count, 1, 1, dbx->file);
    fseek(dbx->file, 1L, SEEK_CUR);

    dbx->info[i].valid = 0;
    
    for (j = 0; j < count; j++) {
      int type = 0;
      unsigned int value = 0;

      fseek(dbx->file, pos, SEEK_SET);
      fread(&type, 1, 1, dbx->file);
      fread(&value, 3, 1, dbx->file);

      /* msb means direct storage */
      offset = (type & 0x80)? 0:(index + 12 + 4 * count + value);

      /* dirt ugly code follows ... */
      switch (type & 0x7f) {
      case 0x00:
	dbx->info[i].message_index = _dbx_read_long(dbx->file, offset, value);
	dbx->info[i].valid |= DBX_MASK_INDEX;
	break;
      case 0x01:
	dbx->info[i].flags = _dbx_read_long(dbx->file, offset, value);
	dbx->info[i].valid |= DBX_MASK_FLAGS;
	break;
      case 0x02:
	dbx->info[i].send_create_time = _dbx_read_date(dbx->file, offset);
	break;
      case 0x03:
	dbx->info[i].body_lines = _dbx_read_long(dbx->file, offset, value);
	dbx->info[i].valid |= DBX_MASK_BODYLINES;	
	break;
      case 0x04:
	dbx->info[i].message_address = _dbx_read_long(dbx->file, offset, value);
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
	dbx->info[i].message_priority = _dbx_read_long(dbx->file, offset, value);
	dbx->info[i].valid |= DBX_MASK_MSGPRIO;	
	break;
      case 0x11:
	dbx->info[i].message_size = _dbx_read_long(dbx->file, offset, value);
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
    _dbx_set_filename(dbx->info + i);
  }
}


static int _dbx_read_index(dbx_t *dbx, long pos)
{
  int i;
  long next_table;
  char ptr_count = 0;
  long index_count;

  if (pos < 0 || pos >= dbx->file_size) {
    return 0;
  }
    
  fseek(dbx->file, pos + 8L, SEEK_SET);
  fread(&next_table, 4, 1, dbx->file);
  fseek(dbx->file, 5L, SEEK_CUR);
  fread(&ptr_count, 1, 1, dbx->file);
  fseek(dbx->file, 2L, SEEK_CUR);
  fread(&index_count, 4, 1, dbx->file);
  
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
    fread(&index_ptr, 4, 1, dbx->file);
    fread(&next_table, 4, 1, dbx->file);
    fread(&index_count, 4, 1, dbx->file);

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
  long index_ptr;
  long item_count;
	
  fseek(dbx->file, INDEX_POINTER, SEEK_SET);
  fread(&index_ptr, 4, 1, dbx->file);

  fseek(dbx->file, ITEM_COUNT, SEEK_SET);
  fread(&item_count, 4, 1, dbx->file);

  if (item_count > 0) 
    return _dbx_read_index(dbx, index_ptr);
  else
    return 0;
}


static void _dbx_init(dbx_t *dbx)
{
  unsigned int signature[4];

  dbx->message_count = 0;
  dbx->capacity = 0;
  dbx->info = NULL;

  fseek(dbx->file, 0L, SEEK_END);
  dbx->file_size = ftell(dbx->file);
  fseek(dbx->file, 0L, SEEK_SET);
  
  fread(signature, 4, 4, dbx->file);

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

  if (dbx->type == DBX_TYPE_EMAIL) {
    _dbx_read_indexes(dbx);
    _dbx_read_info(dbx);
    qsort(dbx->info, dbx->message_count, sizeof(dbx_info_t), (dbx_cmpfunc_t) _dbx_info_cmp);
    _dbx_uniquify_filenames(dbx);
  }
}


dbx_t *dbx_open(char *filename)
{
  dbx_t *dbx = (dbx_t *) calloc(1, sizeof(dbx_t));

  if (dbx) {
    dbx->file = fopen(filename, "rb");
    if (dbx->file == NULL) {
      free(dbx);
      dbx = NULL;
    }
    else {
      _dbx_init(dbx);
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

    free(dbx);
  }
}

char *dbx_message(dbx_t *dbx, int msg_number, long *psize)
{
  long index = 0;
  int size = 0;
  char count = 0;
  long msg_offset = 0;
  long msg_offset_ptr = 0;
  long value = 0;
  unsigned char type = 0;
  unsigned long total_size = 0;
  short block_size = 0;
  long i = 0;
  char *buf = NULL;
 
  if (psize)
    *psize = 0;
  
  if (dbx == NULL || msg_number >= dbx->message_count)
    return NULL;

  index = dbx->info[msg_number].index;

  fseek(dbx->file, index + 4L, SEEK_SET);
  fread(&size, 4, 1, dbx->file);
  fseek(dbx->file, 2L, SEEK_CUR);
  fread(&count, 1, 1, dbx->file);
  fseek(dbx->file, 1L, SEEK_CUR);

  for (i = 0; i < count; i++) {
    type = 0;
    fread(&type, 1, 1, dbx->file);
    value=0;
    fread(&value, 3, 1, dbx->file);
		
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
    fread(&msg_offset, 4, 1, dbx->file);
  }

  buf = NULL;
  i = msg_offset;
  total_size = 0;
  
  while (i != 0) {
    fseek(dbx->file, i + 8L, SEEK_SET);
    block_size=0;
    fread(&block_size, 2, 1, dbx->file);
    fseek(dbx->file, 2L, SEEK_CUR);
    fread(&i, 4, 1, dbx->file);
    total_size += block_size;
    buf = realloc(buf, total_size + 1);
    fread(buf + total_size - block_size, block_size, 1, dbx->file); 
  }

  if (buf)
    buf[total_size]='\0';

  if (psize)
    *psize = total_size;
  
  return buf;
}

