#ifndef _DBX_SYS_H_
#define _DBX_SYS_H_

#ifdef __cplusplus
extern "C" {
#endif

  char **sys_glob(char *parent, char *pattern, int *num_files);
  void sys_glob_free(char **pglob);
  int sys_mkdir(char *parent, char *dir);
  int sys_chdir(char *dir);
  char *sys_getcwd(void);
  long sys_filesize(char *parent, char *filename);
  int sys_delete(char *parent, char *filename);
  
#ifdef __cplusplus
};
#endif

#endif /* _DBX_SYS_H_ */
