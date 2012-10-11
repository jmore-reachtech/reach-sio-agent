#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>

#include "sio_agent.h"


int sioTtyInit(const char *tty_dev)
{
    int fd = -1;

    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CREAD | CLOCAL;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 5;

    if (tty_dev == 0) {
        fd = posix_openpt(O_RDWR | O_NOCTTY);
        if (fd < 0) {
            printf("can't open %s\n", tty_dev);
        } else {
            tcsetattr(fd, TCSANOW, &tio);
            grantpt(fd);
            unlockpt(fd);
            printf("slave port = %s\n", ptsname(fd));
        }
    } else {
        fd = open(tty_dev, O_RDWR);
        if (fd < 0) {
            printf("can't open %s\n", tty_dev);
        } else {
            cfsetospeed(&tio, B115200);
            cfsetispeed(&tio, B115200);
            tcsetattr(fd, TCSANOW, &tio);
        }
    }

    return fd;
}

int sioTtyRead(int fd, char *msgBuff, size_t bufSize, off_t *currPos)
{
    /* Gather entire string, drop CR. */
    char c;
    if (read(fd, &c, 1) > 0) {
        if (c == '\r') {
            int pos = *currPos;
            *currPos = 0;

            msgBuff[pos++] = '\n';
            msgBuff[pos++] = '\0';

            if (sioLocalEchoFlag) {
                write(fd, "\r\n", 2);
            }

            if (sioVerboseFlag) {
                fprintf(stdout, "%s: buff = %s", __FUNCTION__, msgBuff);
            }

            return pos;
        } else if (c == '\n') {
            return 0;
        } else if (c == '\b') {
            /*  If it's BS with nothing in buffer, ignore, else
             *  back up stream, erasing last character typed. */
            if (*currPos > 0) {
                (*currPos)--;
                write(fd, "\b \b", 3);
            }
            return 0;
        } else {
            msgBuff[(*currPos)++] = c;
            if (sioLocalEchoFlag) {
                write(fd, &c, 1);
            }
            return 0;
        }
    } else {
        if (sioVerboseFlag) {
            printf("sio_tty_reader(): error on read()\n");
        }
        return -1;
    }
}


void sioTtyWrite(int serialFd, const char *msgBuff, int buffSize)
{
    if (sioVerboseFlag) {
        fprintf(stdout, "sioTTYWRiter(): got buff %s", msgBuff);
    }

    if (write(serialFd, msgBuff, buffSize) < 0) {
        if (sioVerboseFlag) {
            fprintf(stdout, "sio_tty_writter(): error on write()\n");
        }
    }

    /*  Put a CR in; we may want to revisit how to do this. */
    write(serialFd, "\r", 1);
}
