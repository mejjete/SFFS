/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#ifndef SFFS_ERR_H
#define SFFS_ERR_H

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sffs.h>

#define MAXLINE 4096

void err_sys(sffs_context_t *sffs_ctx, const char *fmt, ...);

void err_dump(sffs_context_t *sffs_ctx, const char *fmt, ...);

void err_msg(sffs_context_t *sffs_ctx, const char *fmt, ...);

void err_no_log();

#endif // SFFS_ERR_H