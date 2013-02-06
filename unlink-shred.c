/*
 * LD_PRELOAD library that runs shred before unlinking files
 * 
 * extra verbose on purpose for testing. use with caution.
 *
 * COMPILE:
 *   gcc -Wall -fPIC -DPIC -c unlink-shred.c
 *   ld -shared -o unlink-shred.so unlink-shred.o -ldl -lc
 * 
 * USAGE:
 *   LD_PRELOAD=./unlink-shred.so unlink test
 * 
 * Based on 
 * http://www.dovecot.org/list/dovecot/2011-September/061130.html 
 * with a few small changes to actually make it work
 * 
 * PUBLIC DOMAIN
 */

#define DEBUG

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#ifdef DEBUG
# define DEBUG_PRINTF(x) append_log x
#else
# define DEBUG_PRINTF(x) do {} while (0)
#endif

void append_log(const char *text,...) {
 time_t currentTime;
 struct tm * timeInfo;
 char timestr[20];

 time( &currentTime );
 timeInfo = localtime( &currentTime );
 strftime( timestr, 20, "%Y-%m-%d %H:%M:%S", timeInfo);
 
 va_list argptr;
 FILE *out = fopen("/tmp/unlink-shred.log", "a");
 fprintf(out, "[%s] ", timestr);
 va_start(argptr, text);
 vfprintf(out, text, argptr);
 va_end(argptr);
 fclose(out);
}

int unlink(const char *path) {
  static int (*libc_unlink)(const char *path) = NULL;
  char *args[3];
  int status;
  struct stat fstatus;

  if (libc_unlink == NULL)
    libc_unlink = dlsym(RTLD_NEXT, "unlink");

  if (lstat(path,&fstatus)==0) {
    if ((fstatus.st_mode & S_IFMT) != S_IFREG) {
       // not a regular file, just unlink
       DEBUG_PRINTF(("%s is not a regular file, not shredding.\n", path));
       return libc_unlink(path);
    }
  } else {
    // nothing lstat can handle, just unlink
    DEBUG_PRINTF(("lstat failed for %s: %s\n", path, strerror(errno)));
    return libc_unlink(path);
  }
  
  if (fstatus.st_nlink>1) {
    DEBUG_PRINTF(("%s has more than one hard link, not shredding\n", path));
    return libc_unlink(path);
  }

  // regular file, last hard link: shred!
  args[0] = "/usr/bin/shred";
  args[1] = path;
  args[2] = NULL;
  switch (fork()) {
   case -1: DEBUG_PRINTF(("fork error\n")); return -1;
   case 0: execve(args[0], args, NULL); 
   default: if (wait(&status) < 0) return -1;
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
   DEBUG_PRINTF(("yay. shredded %s before unlink\n", path));
   return libc_unlink(path);
  }
  DEBUG_PRINTF(("failed to shred %s before unlink\n", path));
  return libc_unlink(path);
}

