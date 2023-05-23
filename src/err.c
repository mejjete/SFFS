/**
 *  SPDX-License-Identifier: MIT
 *  Copyright (c) 2023 Danylo Malapura
*/

#include <stdio.h>
#include <sffs_err.h>

void err_sys(sffs_context_t *sffs_ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(sffs_ctx->log_id, fmt, ap);
    va_end(ap);
    fsync(sffs_ctx->log_id);
    exit(EXIT_FAILURE);
}

void err_dump(sffs_context_t *sffs_ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(sffs_ctx->log_id, fmt, ap);
    va_end(ap);
    fsync(sffs_ctx->log_id);
    abort();
    exit(EXIT_FAILURE);     /* shouldn't get here */
}

void err_msg(sffs_context_t *sffs_ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(sffs_ctx->log_id, fmt, ap);
    va_end(ap);
    fsync(sffs_ctx->log_id);
}

void err_no_log()
{
    exit(EXIT_FAILURE);
}