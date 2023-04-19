#ifndef SFFS_ERR_H
#define SFFS_ERR_H

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAXLINE 4096

void err_sys(const char *fmt, ...);

void err_dump(const char *fmt, ...);

void err_msg(const char *fmt, ...);

void err_no_log();

#endif // SFFS_ERR_H