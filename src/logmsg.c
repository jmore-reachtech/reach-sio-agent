#define _BSD_SOURCE  /* for vsyslog() */

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "sio_agent.h"


/* -1: syslog; >= 0: an open file descriptor */
static FILE *logFile;
static int verboseOn;

/**
 * Sets up the logging for the program.  Three different message destinations 
 * are possible: the system's syslog facility; a named file; and the process' 
 * standard error stream. 
 * 
 * @param ident the ident string supplied to syslog, typically this is the 
 *              program's name
 * @param logToSyslog 
 * @param logFilePath 
 * @param verboseFlag 
 */
void LogOpen(const char *ident, int logToSyslog, const char *logFilePath,
    int verboseFlag)
{
    if (logToSyslog) {
        openlog(ident, 0, LOG_USER);
        logFile = 0;
    } else if (logFilePath != 0) {
        logFile = fopen(logFilePath, "a");
        if (logFile == 0) {
            perror("could not open log file");
            exit(-1);
        }

        /* 
         * set the file to line buffered so it gets updated in a timely
         * manner 
         */ 
        setlinebuf(logFile);
    } else {
        logFile = stderr;
    }

    verboseOn = verboseFlag;
}

void LogMsg(int level, const char *fmt, ...)
{
    /* 
     * only log the message if in verbose mode or the priority is higher than
     * informational
     */ 
    if (verboseOn || (level < LOG_INFO)) {
        va_list ap;
        va_start(ap, fmt);

        if (logFile == 0) {
            /* log to syslog */
            vsyslog(LOG_USER | level, fmt, ap);
        } else {
            /* log to an open descriptor such as stdout or a file */
            vfprintf(logFile, fmt, ap);
        }

        va_end(ap);
    }
}

