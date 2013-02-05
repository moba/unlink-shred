/*
 * LD_PRELOAD library that runs shred before unlinking files
 * 
 * COMPILE:
 *   gcc -Wall -fPIC -DPIC -c unlink-shred.c
 *   ld -shared -o unlink-shred.so unlink-shred.o -ldl
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

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>

int unlink(const char *path) {
  static int (*libc_unlink)(const char *path) = NULL;
  char *args[3];
  int status;

  if (libc_unlink == NULL)
    libc_unlink = dlsym(RTLD_NEXT, "unlink");

  args[0] = "/usr/bin/shred";
  args[1] = path;
  args[2] = NULL;
  switch (fork()) {
   case -1: printf("fork error\n"); return -1;
   case 0: execve(args[0], args, NULL); 
   default: if (wait(&status) < 0) return -1;
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
   printf("shredded %s before unlink\n", path);
   return libc_unlink(path);
  }
  printf("failed to shred %s before unlink\n", path);
  return libc_unlink(path);
}

