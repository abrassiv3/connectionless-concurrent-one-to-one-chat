// utils.c — File I/O utilities with flock() for concurrent safety
//
// flock() additions:
//   append_record() : acquires an EXCLUSIVE lock before writing,
//                     releases it after. Prevents two child processes
//                     from writing to the same file simultaneously.
//
//   get_next_id()   : acquires a SHARED lock before reading the file
//                     size. Prevents a concurrent write from changing
//                     the size mid-read, which would produce a wrong ID.
//
//   log_event()     : acquires an EXCLUSIVE lock on server.log before
//                     writing. Prevents interleaved log lines from
//                     multiple child processes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>     // getpid()
#include <sys/file.h>   // flock()
#include "server.h"

int get_next_id(const char *filename, size_t record_size) {
    printf("[UTIL] get_next_id(): Opening '%s' to calculate next ID.\n", filename);

    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("[UTIL] get_next_id(): '%s' does not exist yet. Returning ID 1.\n", filename);
        return 1;
    }

    // Acquire shared lock — allows concurrent reads, blocks exclusive writes
    printf("[UTIL] get_next_id(): Acquiring SHARED lock on '%s'...\n", filename);
    flock(fileno(file), LOCK_SH);
    printf("[UTIL] get_next_id(): Shared lock acquired.\n");

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    int next_id = (size / record_size) + 1;

    flock(fileno(file), LOCK_UN);
    printf("[UTIL] get_next_id(): Lock released. Next ID = %d\n", next_id);

    fclose(file);
    return next_id;
}

int append_record(const char *filename, void *record, size_t record_size) {
    printf("[UTIL] append_record(): Opening '%s' for writing (pid=%d).\n",
           filename, getpid());

    FILE *file = fopen(filename, "ab");
    if (!file) {
        printf("[UTIL] append_record(): ERROR — could not open '%s'.\n", filename);
        return -1;
    }

    // Acquire exclusive lock — blocks ALL other readers and writers.
    // This is the critical section: only one process may write at a time.
    printf("[UTIL] append_record(): Acquiring EXCLUSIVE lock on '%s' (pid=%d)...\n",
           filename, getpid());
    flock(fileno(file), LOCK_EX);
    printf("[UTIL] append_record(): Exclusive lock acquired (pid=%d). Writing record...\n",
           getpid());

    fwrite(record, record_size, 1, file);

    // Release lock immediately after write completes
    flock(fileno(file), LOCK_UN);
    printf("[UTIL] append_record(): Write complete. Lock released (pid=%d).\n", getpid());

    fclose(file);
    return 0;
}

void log_event(const char *level, const char *format, ...) {
    FILE *log_file = fopen("server.log", "a");
    if (!log_file) {
        printf("[!] CRITICAL: Could not open server.log for writing!\n");
        return;
    }

    // Exclusive lock on the log file — prevents interleaved lines
    // from multiple child processes writing simultaneously
    flock(fileno(log_file), LOCK_EX);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    fprintf(log_file, "[%s] [%s] [pid=%d] ", time_str, level, getpid());
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fprintf(log_file, "\n");

    flock(fileno(log_file), LOCK_UN);
    fclose(log_file);

    // Mirror to stdout — pid shown so you can trace which child logged what
    printf("[%s] [%s] [pid=%d] ", time_str, level, getpid());
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
