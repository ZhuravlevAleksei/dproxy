#ifndef LOGGER_H
#define LOGGER_H

#include <threads.h>
#include <time.h>


#define SERVER_LOG_TIMEOUT 10000000L // 10ms
#define MAIN_LOG_TIMEOUT 1000000L    // 1ms

void lock_time_init(struct timespec* lock_time, long t);
void logger_init();
void error_log(struct timespec* l_time, char* format, ...);
void info_log(struct timespec* l_time, char* format, ...);

#endif /* #ifndef LOGGER_H */
