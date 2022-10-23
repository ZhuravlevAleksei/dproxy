#include "logger.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

static mtx_t logs_output_mtx;
static bool logger_is_init;

void logger_init()
{
    if (mtx_init(&logs_output_mtx, mtx_timed) != thrd_success)
    {
        logger_is_init = false;
    }
    else
    {
        logger_is_init = true;
    }
}

void lock_time_init(struct timespec *lock_time, long t)
{
    timespec_get(lock_time, TIME_UTC);

    lock_time->tv_sec += 0;
    lock_time->tv_nsec += t;
}

void error_log(struct timespec *l_time, char *format, ...)
{
    va_list args;

    if (!logger_is_init)
    {
        return;
    }


    if (mtx_timedlock(&logs_output_mtx, l_time) == thrd_success)
    {
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fflush(stderr);
        mtx_unlock(&logs_output_mtx);
    }

}

void info_log(struct timespec *l_time, char *format, ...)
{
    va_list args;
    
    if (!logger_is_init)
    {
        return;
    }

    if (mtx_timedlock(&logs_output_mtx, l_time) == thrd_success)
    {
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
        fflush(stdout);
        mtx_unlock(&logs_output_mtx);
    }
}
