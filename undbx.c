#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "dbxread.h"
#include "dbxsys.h"

static int _str_cmp(const char **ia, const char **ib)
{
  return strcmp(*ia, *ib);
}

static void _save_message(char *dir, char *filename, char *message, long size)
{
  FILE *eml = NULL;
  char *cwd = NULL;
  size_t b = 0;
  int rc = 0;

  cwd = sys_getcwd();
  if (cwd == NULL) {
    perror("_save_message (sys_getcwd)");
    return;
  }
  
  rc = sys_chdir(dir);
  if (rc != 0) {
    perror("_save_message (sys_chdir)");
    return;
  }

  eml = fopen(filename, "wb");

  sys_chdir(cwd);
  free(cwd);
  cwd = NULL;
  
  if (eml == NULL) {
    perror("_save_message (fopen)");    
    return;
  }

  b = fwrite(message, 1, size, eml);
  if (b != size) {
    perror("_save_message (fwrite)");
    return;
  }

  printf("Extracted message: %s\n", filename);
  fflush(stdout);
  fclose(eml);
}

static void _maybe_save_message(dbx_t *dbx, int imessage, char *dir, int force)
{
  dbx_info_t *info = dbx->info + imessage;
  long size = 0;
  long message_size = 0;
  char *message = NULL;

  if (!force) 
    size = sys_filesize(dir, info->filename);
  
  if (force || (info->valid & DBX_MASK_MSGSIZE) == 0 || size != info->message_size) {
    message = dbx_message(dbx, imessage, &message_size);
    if (force || (size != message_size)) 
      _save_message(dir, info->filename, message, message_size);
    free(message);
  }
}

static int _undbx(char *dbx_dir, char *out_dir, char *dbx_file)
{
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

  eml_dir = strdup(dbx_file);
  eml_dir[strlen(eml_dir) - 4] = '\0';
  rc = sys_mkdir(out_dir, eml_dir);
  if (rc != 0) {
    fprintf(stderr, "error: can't create directory %s/%s\n", out_dir, eml_dir);
    return -1;
  }

  printf("Extracting %d messages from %s to %s/%s ... \n", dbx->message_count, dbx_file, out_dir, eml_dir);
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
      _maybe_save_message(dbx, imessage, eml_dir, 1); 
      imessage++;
      no_more_messages = (imessage == dbx->message_count);
    }
    else if (cond == 0) {
      /* message found on disk: extract from dbx if modified */
      _maybe_save_message(dbx, imessage, eml_dir, 0);
      imessage++;
      no_more_messages = (imessage == dbx->message_count);	
      ifile++;
      no_more_files = (ifile == num_eml_files);
    }
    else {
      /* file on disk not found in dbx: delete from disk */
      sys_delete(eml_dir, eml_files[ifile]);
      printf("Deleted message %s\n", eml_files[ifile]);
      ifile++;
      no_more_files = (ifile == num_eml_files);	
    }
  }

  sys_glob_free(eml_files);

  free(eml_dir);
  eml_dir = NULL;
  
  dbx_close(dbx);

  sys_chdir(cwd);
  free(cwd);
  cwd = NULL;

  return 0;
}

static void _usage(char *prog)
{
  fprintf(stderr, "Usage: %s <DBX-DIRECTORY> <OUTPUT-DIRECTORY>\n", prog);
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
  
  if (argc != 3)
    _usage(argv[0]);

  dbx_dir = argv[1];
  out_dir = argv[2];

  dbx_files = sys_glob(dbx_dir, "*.dbx", &num_dbx_files);

  for(n = 0; n < num_dbx_files; n++) {
    if (_undbx(dbx_dir, out_dir, dbx_files[n]))
      fail++;
  }

  sys_glob_free(dbx_files);

  printf("Extracted %d out of %d DBX files.\n", n - fail, n);
  
  return EXIT_SUCCESS;
}
