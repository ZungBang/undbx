/*
    UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
    Copyright (C) 2008-2010 Avi Rozen <avi.rozen@gmail.com>

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
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "emlread.h"

static char *_eml_next_line(char *message)
{
  char *line = message;
  while (*line && *line != '\n') 
    line++;
  return *line? ++line:NULL;
}

static int _eml_islast(char *line)
{
  return (line[0] == '\n' || (line[0] == '\r' && line[1] == '\n'))? 1:0;
}

static char *_eml_isheader(char *line)
{
  char *header = line;
  while (*header && *header != '\n' && *header != ':')
    header++;
  if (*header == ':') {
    char *result = NULL;
    char *lwr = NULL;
    *header = '\0';
    result = strdup(line);
    *header = ':';
    lwr = result;
    for (lwr = result; *lwr; lwr++)
      *lwr = tolower(*lwr);
    return result;
  }
  return NULL;
}


static char *_eml_header_add_line(eml_t *eml, char *line)
{
  char c = 0;
  char *next = _eml_next_line(line);
  if (next) {
    char *tail = NULL;
    int last = 0;
    int first = 0;
    int length = strlen(eml->values[eml->count - 1]);
    
    /* get string */
    c = *next;
    *next = '\0';
    tail = strdup(line);
    *next = c;
    /* trim trailing whitespace */
    for (last = strlen(tail); last; last--) {
      if (isspace(tail[last - 1]))
        tail[last - 1] = '\0';
      else
        break;
    }
    /* find first non-space char */
    while (first < last && isspace(tail[first]))
      first++;
    /* backup one char */
    if (length && first) {
      first--;
      tail[first] = ' ';
    }
    /* printf("\033[1;31;48m%s\033[0m: first=%d last=%d %s", eml->keys[eml->count - 1], first, last, tail); */
    eml->values[eml->count - 1] = (char *)realloc(eml->values[eml->count - 1],
                                                  length + strlen(tail + first) + 1);
    strcat(eml->values[eml->count - 1], tail + first);
    free(tail);
  }
  return next;
}

static char *_eml_header_add_entry(eml_t *eml, char *headerseen, char *line)
{
  eml->count++;
  eml->keys = (char **)realloc(eml->keys, eml->count * sizeof(char *));
  eml->keys[eml->count - 1] = headerseen;
  eml->values = (char **)realloc(eml->values, eml->count * sizeof(char *));
  eml->values[eml->count - 1] = strdup("");
  return _eml_header_add_line(eml, line + strlen(headerseen) + 1);
}

eml_t *eml_init(char *message)
{
  char *headerseen = NULL;
  char *line = message;

  eml_t *eml = (eml_t *)calloc(1, sizeof(eml_t));
  
  /* skip unix From name time lines */
  if (strncmp(line, "From ", 5) == 0)
    line = _eml_next_line(line);
  while (line) {
    /* a continuation line */
    if (headerseen && (line[0] == ' ' || line[0] == '\t')) {
      line = _eml_header_add_line(eml, line);
      continue;
    }
    /* last line */
    else if (_eml_islast(line))
      break;
    headerseen = _eml_isheader(line);
    if (headerseen) {
      /* legal header line */
      line = _eml_header_add_entry(eml, headerseen, line);
      continue;
    }
    else {
      /* not a header line: stop */
      break;
    }
  }

  return eml;
}

void eml_free(eml_t *eml)
{
  if (eml) {
    if (eml->count) {
      int i = 0;
      
      for (i = 0; i < eml->count; i++) {
        free(eml->keys[i]);
        free(eml->values[i]);
      }
      free(eml->keys);
      free(eml->values);
    }
    free(eml);
  }
}

char *eml_get_header(eml_t *eml, char *header)
{
  int i = 0;
  for (i = 0; i < eml->count; i++) {
    if (strcmp(eml->keys[i], header) == 0)
      return eml->values[i];
  }
  return NULL;
}

time_t eml_get_time(eml_t *eml, char *header)
{
  /*char *date = eml_get_header(eml, header);*/
  return 0;
}
