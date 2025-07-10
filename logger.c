#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>

static FILE *log_file = NULL;
static LogLevel current_level = LOG_INFO;

void init_logger(const char *filename) {
    log_file = fopen(filename, "a");
    if (!log_file) {
        perror("Logger failed to open file");
        exit(EXIT_FAILURE);
    }
}

void set_log_level(LogLevel level) {
    current_level = level;
}

static const char* level_str(LogLevel level) {
    switch (level) {
        case LOG_INFO: return "INFO";
        case LOG_WARN: return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

static void write_log(LogLevel level, const char *fmt, va_list args) {
    if (level > current_level) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    fprintf(log_file, "[%s] %s: ", timestamp, level_str(level));
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    fflush(log_file);
}

void log_info(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    write_log(LOG_INFO, fmt, args);
    va_end(args);
}
void log_warn(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    write_log(LOG_WARN, fmt, args);
    va_end(args);
}
void log_error(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    write_log(LOG_ERROR, fmt, args);
    va_end(args);
}
void log_debug(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    write_log(LOG_DEBUG, fmt, args);
    va_end(args);
}

void close_logger() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}
