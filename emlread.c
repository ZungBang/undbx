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
#include <time.h>
#include "emlread.h"

enum {
  EOK = 0,
  EPARSE,
  ENOMEM,
  EBAD2047,
  EINVAL,
  EFULL
};

static int _eml_str_append_n (char **to, const char *from, size_t n)
{
  size_t l = 0;

  if (!to) 
    return EOK;

  if (*to) {
    char *bigger;
    l = strlen (*to);
    bigger = realloc (*to, l + n + 1);
    if (!bigger)
      return ENOMEM;
    *to = bigger;
  }
  else
    *to = malloc (n + 1);

  strncpy (&to[0][l], from, n);
  to[0][l + n] = 0;

  return EOK;
}

static int _eml_str_append_char (char **to, char c)
{
  return _eml_str_append_n (to, &c, 1);
}

static int _eml_str_append_range (char **to, const char *b, const char *e)
{
  return _eml_str_append_n (to, b, e - b);
}

static void _eml_str_free (char **s)
{
  if (s && *s) {
    free (*s);
    *s = 0;
  }
}

static int _eml_parse822_is_char (char c)
{
  return isascii (c);
}

static int _eml_parse822_is_digit (char c)
{
  return isdigit ((unsigned) c);
}

static int _eml_parse822_is_ctl (char c)
{
  return iscntrl ((unsigned) c) || c == 127 /* DEL */ ;
}

static int _eml_parse822_is_space (char c)
{
  return c == ' ';
}

static int _eml_parse822_is_htab (char c)
{
  return c == '\t';
}

static int _eml_parse822_is_lwsp_char (char c)
{
  return _eml_parse822_is_space (c) || _eml_parse822_is_htab (c);
}

static int _eml_parse822_is_special (char c)
{
  return strchr ("()<>@,;:\\\".[]", c) ? 1 : 0;
}

static int _eml_parse822_is_atom_char_ex (char c)
{
  return !_eml_parse822_is_special (c)
    && !_eml_parse822_is_space (c)
    && !_eml_parse822_is_ctl (c);
}

static int _eml_parse822_is_atom_char (char c)
{
  return _eml_parse822_is_char (c) && _eml_parse822_is_atom_char_ex (c);
}

static int _eml_parse822_digits (const char **p, const char *e, int min, int max, int *digits)
{
  const char *save = *p;
  int i = 0;

  *digits = 0;

  while (*p < e && _eml_parse822_is_digit (**p)) {
    *digits = *digits * 10 + **p - '0';
    *p += 1;
    ++i;
    if (max != 0 && i == max) {
      break;
    }
  }
  if (i < min) {
    *p = save;
    return EPARSE;
  }

  return EOK;
}

static int _eml_parse822_skip_nl (const char **p, const char *e)
{
  const char *s = *p;

  if ((&s[1] < e) && s[0] == '\r' && s[1] == '\n') {
    *p += 2;
    return EOK;
  }

  if ((&s[0] < e) && s[0] == '\n') {
    *p += 1;
    return EOK;
  }

  return EPARSE;
}


static int _eml_parse822_skip_lwsp_char (const char **p, const char *e)
{
  if (*p < e && _eml_parse822_is_lwsp_char (**p)) {
    *p += 1;
    return EOK;
  }
  return EPARSE;
}

static int _eml_parse822_skip_lwsp (const char **p, const char *e)
{
  int space = 0;

  while (*p != e) {
    const char *save = *p;

    if (_eml_parse822_skip_lwsp_char (p, e) == EOK) {
      space = 1;
      continue;
    }
    if (_eml_parse822_skip_nl (p, e) == EOK) {
      if (_eml_parse822_skip_lwsp_char (p, e) == EOK)
        continue;
      *p = save;
      return EPARSE;
    }
    break;
  }
  return space ? EOK : EPARSE;
}

static int _eml_parse822_quoted_pair (const char **p, const char *e, char **qpair)
{
  int rc;

  if ((e - *p) < 2)
    return EPARSE;

  if (**p != '\\')
    return EPARSE;

  if ((rc = _eml_str_append_char (qpair, *(*p + 1))))
    return rc;

  *p += 2;
  return EOK;
}

static int _eml_parse822_special (const char **p, const char *e, char c)
{
  _eml_parse822_skip_lwsp (p, e);

  if ((*p != e) && **p == c) {
    *p += 1;
    return EOK;
  }
  return EPARSE;
}

static int _eml_parse822_comment (const char **p, const char *e, char **comment)
{
  const char *save = *p;
  int rc;

  if ((rc = _eml_parse822_special (p, e, '(')))
    return rc;

  while (*p != e) {
    char c = **p;

    if (c == ')') {
      *p += 1;
      return EOK;
    }
    else if (c == '(') {
      rc = _eml_parse822_comment (p, e, comment);
    }
    else if (c == '\\'){
      rc = _eml_parse822_quoted_pair (p, e, comment);
    }
    else if (c == '\r') {
      *p += 1;
    }
    else if (_eml_parse822_is_char (c)) {
      rc = _eml_str_append_char (comment, c);
      *p += 1;
    }
    else {
      *p += 1;
    }
    if (rc != EOK)
      break;
  }

  if (*p == e)
    rc = EPARSE;

  *p = save;
  return rc;
}


static int _eml_parse822_skip_comments (const char **p, const char *e)
{
  int status;

  while ((status = _eml_parse822_comment (p, e, 0)) == EOK)
    ;

  return EOK;
}


static int _eml_parse822_atom (const char **p, const char *e, char **atom)
{
  const char *save = *p;
  int rc = EPARSE;

  _eml_parse822_skip_comments (p, e);

  save = *p;

  while ((*p != e) && (**p == '.' || _eml_parse822_is_atom_char (**p))) {
    rc = _eml_str_append_char (atom, **p);
    *p += 1;
    if (rc != EOK) {
      *p = save;
      break;
    }
  }
  return rc;
}

static int _eml_parse822_day (const char **p, const char *e, int *day)
{
  const char *days[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    NULL
  };

  int d;

  _eml_parse822_skip_comments (p, e);

  if ((e - *p) < 3)
    return EPARSE;

  for (d = 0; days[d]; d++) {
    if (strncasecmp (*p, days[d], 3) == 0) {
      *p += 3;
      if (day)
        *day = d;
      return EOK;
    }
  }
  return EPARSE;
}

static int _eml_parse822_date (const char **p, const char *e, int *day, int *mon, int *year)
{
  const char *mons[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
    NULL
  };

  const char *save = *p;
  int rc = EOK;
  int m = 0;
  int yr = 0;
  const char *yrbeg = 0;

  _eml_parse822_skip_comments (p, e);

  if ((rc = _eml_parse822_digits (p, e, 1, 2, day))) {
    *p = save;
    return rc;
  }

  _eml_parse822_skip_comments (p, e);

  if ((e - *p) < 3)
    return EPARSE;

  for (m = 0; mons[m]; m++) {
    if (strncasecmp (*p, mons[m], 3) == 0) {
      *p += 3;
      if (mon)
        *mon = m;
      break;
    }
  }

  if (!mons[m]) {
    *p = save;
    return EPARSE;
  }

  _eml_parse822_skip_comments (p, e);

  yrbeg = *p;

  if ((rc = _eml_parse822_digits (p, e, 2, 4, &yr))) {
    *p = save;
    return rc;
  }

  switch (*p - yrbeg) {
  case 2:
    if (yr >= 0 && yr <= 49) {
      yr += 2000;
      break;
    }
  case 3:
    yr += 1900;
    break;
  }

  if (year)
    *year = yr - 1900;

  return EOK;
}

static int _eml_parse822_time (const char **p, const char *e,
                               int *hour, int *min, int *sec, int *tz, const char **tz_name)
{
  struct {
    const char *tzname;
    int tz;
  }
  tzs[] =
    {
      { "UT",   0 * 60 * 60 },
      { "UTC",  0 * 60 * 60 },
      { "GMT",  0 * 60 * 60 },
      { "EST", -5 * 60 * 60 },
      { "EDT", -4 * 60 * 60 },
      { "CST", -6 * 60 * 60 },
      { "CDT", -5 * 60 * 60 },
      { "MST", -7 * 60 * 60 },
      { "MDT", -6 * 60 * 60 },
      { "PST", -8 * 60 * 60 },
      { "PDT", -7 * 60 * 60 },
      { NULL, 0}
    };

  const char *save = *p;
  int rc = EOK;
  int z = 0;
  char *zone = NULL;

  _eml_parse822_skip_comments (p, e);

  if ((rc = _eml_parse822_digits (p, e, 1, 2, hour))) {
    *p = save;
    return rc;
  }

  if ((rc = _eml_parse822_special (p, e, ':'))) {
    *p = save;
    return rc;
  }

  if ((rc = _eml_parse822_digits (p, e, 1, 2, min))) {
    *p = save;
    return rc;
  }

  if ((rc = _eml_parse822_special (p, e, ':'))) {
    *sec = 0;
  }
  else if ((rc = _eml_parse822_digits (p, e, 1, 2, sec))) {
    *p = save;
    return rc;
  }

  _eml_parse822_skip_comments (p, e);

  if ((rc = _eml_parse822_atom (p, e, &zone))) {
    if (tz)
      *tz = 0;
    return EOK;
  }

  for (; tzs[z].tzname; z++) {
    if (strcasecmp (zone, tzs[z].tzname) == 0)
      break;
  }
  if (tzs[z].tzname) {
    if (tz_name)
      *tz_name = tzs[z].tzname;

    if (tz)
      *tz = tzs[z].tz;
  }
  else if (strlen (zone) > 5 || strlen (zone) < 4) {
    if (*tz)
      *tz = 0; 
  }
  else {
    int hh;
    int mm;
    int sign;
    char *zp = zone;

    switch (zp[0]) {
    case '-':
      sign = -1;
      zp++;
      break;
    case '+':
      sign = +1;
      zp++;
      break;
    default:
      sign = 1;
      break;
    }

    if (strspn (zp, "0123456789") == 4) {
      hh = (zone[1] - '0') * 10 + (zone[2] - '0');
      mm = (zone[3] - '0') * 10 + (zone[4] - '0');
    }
    else {
      hh = mm = 0;
    }
    if (tz)
      *tz = sign * (hh * 60 * 60 + mm * 60);
  }

  _eml_str_free (&zone);

  return EOK;
}

int
_eml_parse822_date_time (const char **p, const char *e, struct tm *tm, time_t *ptzoffset)
{
  const char *save = *p;
  int rc = 0;

  int wday = 0;

  int mday = 0;
  int mon = 0;
  int year = 0;

  int hour = 0;
  int min = 0;
  int sec = 0;

  int tzoffset = 0;
  const char *tz_name = 0;

  if ((rc = _eml_parse822_day (p, e, &wday))) {
    if (rc != EPARSE)
      return rc;
  }
  else {
    _eml_parse822_skip_comments (p, e);

    if ((rc = _eml_parse822_special (p, e, ','))) {
      *p = save;
      return rc;
    }
  }

  if ((rc = _eml_parse822_date (p, e, &mday, &mon, &year))) {
    *p = save;
    return rc;
  }
  if ((rc = _eml_parse822_time (p, e, &hour, &min, &sec, &tzoffset, &tz_name))) {
    *p = save;
    return rc;
  }

  if (tm) {
    memset (tm, 0, sizeof (*tm));
    tm->tm_wday = wday;
    tm->tm_mday = mday;
    tm->tm_mon = mon;
    tm->tm_year = year;
    tm->tm_hour = hour;
    tm->tm_min = min;
    tm->tm_sec = sec;
    tm->tm_isdst = -1;
  }
  
  if (ptzoffset)
    *ptzoffset = tzoffset;


  return EOK;
}

static int _eml_parse822_field_name (const char **p, const char *e, char **fieldname)
{
  const char *save = *p;

  char *fn = NULL;

  while (*p != e) {
    char c = **p;

    if (!_eml_parse822_is_char (c))
      break;
    if (_eml_parse822_is_ctl (c))
      break;
    if (_eml_parse822_is_space (c))
      break;
    if (c == ':')
      break;

    _eml_str_append_char (&fn, c);
    *p += 1;
  }

  if (!fn) {
    *p = save;
    return EPARSE;
  }
  _eml_parse822_skip_comments (p, e);

  if (_eml_parse822_special (p, e, ':') != EOK) {
    *p = save;
    if (fn)
      free (fn);
    return EPARSE;
  }

  *fieldname = fn;

  return EOK;
}

static int _eml_parse822_field_body (const char **p, const char *e, char **fieldbody)
{
  char *fb = NULL;

  for (;;) {
    const char *eol = *p;
    while (eol != e) {
      if (eol[0] == '\r' && (eol + 1) != e && eol[1] == '\n')
        break;
      ++eol;
    }
    _eml_str_append_range (&fb, *p, eol);
    *p = eol;
    if (eol == e)
      break;
    *p += 2;
    if (*p == e)
      break;
    if (**p != ' ' && **p != '\t')
      break;
  }

  *fieldbody = fb;

  return EOK;
}


static int _eml_b64_input (char c)
{
  const char table[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int i;

  for (i = 0; i < 64; i++) {
    if (table[i] == c)
      return i;
  }
  return -1;
}

static int _eml_base64_decode (const char *iptr, size_t isize, char *optr, size_t osize, size_t *nbytes)
{
  int i = 0, tmp = 0, pad = 0;
  size_t consumed = 0;
  unsigned char data[4];

  *nbytes = 0;
  while (consumed < isize && (*nbytes)+3 < osize) {
    while (( i < 4 ) && (consumed < isize)) {
      tmp = _eml_b64_input (*iptr++);
      consumed++;
      if (tmp != -1)
        data[i++] = tmp;
      else if (*(iptr-1) == '=') {
        data[i++] = '\0';
        pad++;
      }
    }

    if (i == 4) {
      *optr++ = (data[0] << 2) | ((data[1] & 0x30) >> 4);
      *optr++ = ((data[1] & 0xf) << 4) | ((data[2] & 0x3c) >> 2);
      *optr++ = ((data[2] & 0x3) << 6) | data[3];
      (*nbytes) += 3 - pad;
    }
    else {
      consumed -= i;
      return consumed;
    }
    i = 0;
  }
  return consumed;
}


static int _eml_q_decode (const char *iptr, size_t isize, char *optr, size_t osize, size_t *nbytes)
{
  char c;
  size_t consumed = 0;
  
  *nbytes = 0;
  while (consumed < isize && *nbytes < osize) {
    c = *iptr++;

    if (c == '=') {
      if (consumed + 2 >= isize)
        break;
      else {
        char chr[3];
        int  new_c;
              
        chr[2] = 0;
        chr[0] = *iptr++;
        if (chr[0] != '\n') {
          chr[1] = *iptr++;
          new_c = strtoul (chr, NULL, 16);
          *optr++ = new_c;
          (*nbytes)++;
          consumed += 3;
        }
        else
          consumed += 2;
      }
    }
    else if (c == '\r') {
      if (consumed + 1 >= isize)
        break;
      else {
        iptr++;
        *optr++ = '\n';
        (*nbytes)++;
        consumed += 2;
      }
    }
    else if (c == '_') {
      *optr++ = ' ';
      (*nbytes)++;
      consumed++;
    }
    else {
      *optr++ = c;
      (*nbytes)++;
      consumed++;
    }
  }       
  return consumed;
}

static int _eml_realloc_buffer (char **bufp, size_t *bufsizep, size_t incr)
{
  size_t newsize = *bufsizep + incr;
  char *newp = realloc (*bufp, newsize);
  if (newp == NULL)
    return 1;
  *bufp = newp;
  *bufsizep = newsize;
  return 0;
}

static int _eml_getword (char **pret, const char **pstr, int delim)
{
  size_t len;
  char *ret;
  const char *start = *pstr;
  const char *end = strchr (start, delim);

  free (*pret);
  if (!end)
    return EBAD2047;
  len = end - start;
  ret = malloc (len + 1);
  if (!ret)
    return ENOMEM;
  memcpy (ret, start, len);
  ret[len] = 0;
  *pstr = end + 1;
  *pret = ret;
  return 0;
}
    
static int _eml_rfc2047_decode (const char *input, char **ptostr)
{
  int status = 0;
  const char *fromstr;
  char *buffer;
  size_t bufsize;
  size_t bufpos;
  size_t run_count = 0;
  char *fromcode = NULL;
  char *encoding_type = NULL;
  char *encoded_text = NULL;

#define BUFINC 128  
#define CHKBUF(count) do {                              \
    if (bufpos+count >= bufsize)                        \
      {                                                 \
        size_t s = bufpos + count - bufsize;            \
        if (s < BUFINC)                                 \
          s = BUFINC;                                   \
        if (_eml_realloc_buffer (&buffer, &bufsize, s)) \
          {                                             \
            free (buffer);                              \
            free (fromcode);                            \
            free (encoding_type);                       \
            free (encoded_text);                        \
            return ENOMEM;                              \
          }                                             \
      }                                                 \
  } while (0) 
  
  if (!input)
    return EINVAL;
  if (!ptostr)
    return EFULL;

  fromstr = input;

  bufsize = strlen (fromstr) + 1;
  buffer = malloc (bufsize);
  if (buffer == NULL)
    return ENOMEM;
  bufpos = 0;
  
  while (*fromstr) {
    if (strncmp (fromstr, "=?", 2) == 0) {
      int decoded = 0;
      size_t nbytes = 0;
      size_t size;
      const char *sp = fromstr + 2;
      char *decoded_text = NULL;
          
      status = _eml_getword (&fromcode, &sp, '?');
      if (status)
        break;
      status = _eml_getword (&encoding_type, &sp, '?');
      if (status)
        break;
      status = _eml_getword (&encoded_text, &sp, '?');
      if (status)
        break;
      if (sp == NULL || sp[0] != '=') {
        status = EBAD2047;
        break;
      }
      
      size = strlen (encoded_text);
      decoded_text = (char *)calloc(size, sizeof(char));

      switch (encoding_type[0]) {
      case 'b':
      case 'B':
        decoded = (_eml_base64_decode(encoded_text, size, decoded_text, size, &nbytes) && nbytes);
      break;
             
      case 'q': 
      case 'Q':
        decoded = (_eml_q_decode(encoded_text, size, decoded_text, size, &nbytes) && nbytes);
      break;

      default:
        status = EBAD2047;
        break;
      }
          
      if (status != 0)
        break;

      if (decoded) {
        CHKBUF (nbytes); 
        memcpy (buffer + bufpos, decoded_text, nbytes);
        bufpos += nbytes; 
      } 
          
      fromstr = sp + 1;
      run_count = 1;
    }
    else if (run_count) {
      if (*fromstr == ' ' || *fromstr == '\t') {
        run_count++;
        fromstr++;
        continue;
      }
      else {
        if (--run_count) {
          CHKBUF (run_count);
          memcpy (buffer + bufpos, fromstr - run_count, run_count);
          bufpos += run_count;
          run_count = 0;
        }
        CHKBUF (1);
        buffer[bufpos++] = *fromstr++;
      }
    }
    else {
      CHKBUF (1);
      buffer[bufpos++] = *fromstr++;
    }
  }
  
  if (*fromstr) {
    size_t len = strlen (fromstr);
    CHKBUF (len);
    memcpy (buffer + bufpos, fromstr, len);
    bufpos += len;
  }

  CHKBUF (1);
  buffer[bufpos++] = 0;
  
  free (fromcode);
  free (encoding_type);
  free (encoded_text);
  
  *ptostr = realloc (buffer, bufpos);
  return status;
}

static time_t _eml_mktime_tz(struct tm *tm, int tzoffset)
{
  time_t t; /* utc time */
  
  int y = tm->tm_year + 1900, m = tm->tm_mon + 1, d = tm->tm_mday;
  
  if (m < 3) {
    m += 12;
    y--;
  }
  
  t = 86400 *
    (d + (153 * m - 457) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 719469);
  
  t += 3600 * tm->tm_hour + 60 * tm->tm_min + tm->tm_sec;
  
  return t - tzoffset;
}

void eml_parse(char *message, char **subject, char **from, char **to, time_t *timestamp)
{
  char *pmessage = message;
  char *pstop = pmessage + strlen(pmessage);
  char *pname = NULL;
  char *pbody = NULL;
  
  while (_eml_parse822_field_name((const char **)&pmessage, pstop, &pname) == EOK &&
         _eml_parse822_field_body((const char **)&pmessage, pstop, &pbody) == EOK) {
    /* printf("\033[1;31;48m%s\033[0m: %s\n", pname, pbody); */
    if (strcasecmp(pname, "subject") == 0) {
      if (_eml_rfc2047_decode(pbody, subject) != EOK) {
        free(*subject);
        *subject = NULL;
      }
    }
    else if (strcasecmp(pname, "from") == 0) {
      if (_eml_rfc2047_decode(pbody, from) != EOK) {
        free(*from);
        *from = NULL;
      }
    }
    else if (strcasecmp(pname, "to") == 0) {
      if (_eml_rfc2047_decode(pbody, to) != EOK) {
        free(*to);
        *to = NULL;
      }
    }
    else if (strcasecmp(pname, "date") == 0) {
      char *pdate = pbody;
      struct tm tm;
      time_t tzoffset = 0;
      if (_eml_parse822_date_time((const char **)&pdate, pbody + strlen(pbody), &tm, &tzoffset) == EOK) 
        *timestamp = _eml_mktime_tz(&tm, tzoffset);
    }
    free(pbody);
    free(pname);
  }
}
