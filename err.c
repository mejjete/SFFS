#include "sffs_err.h"
#include "sffs_context.h"
#include <stdio.h>

void err_sys(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(sffs_ctx.log_id, fmt, ap);
    va_end(ap);
    fsync(sffs_ctx.log_id);
    exit(EXIT_FAILURE);
}

void err_dump(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(sffs_ctx.log_id, fmt, ap);
    va_end(ap);
    fsync(sffs_ctx.log_id);
    abort();
    exit(EXIT_FAILURE);     /* shouldn't get here */
}

void err_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(sffs_ctx.log_id, fmt, ap);
    va_end(ap);
    fsync(sffs_ctx.log_id);
}

void err_no_log()
{
    exit(EXIT_FAILURE);
}