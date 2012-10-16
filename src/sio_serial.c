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

static int sioLocalEchoFlag;
static speed_t sioTtyRate;

void sioTtySetParams(int localEcho, unsigned int serialRate)
{
    static const struct {
        unsigned int asUint; speed_t asSpeed;
    } speedTable[] = {
        {   1200,   B1200 },
        {   9600,   B9600 },
        {  19200,  B19200 },
        {  38400,  B38400 },
        {  57600,  B57600 },
        { 115200, B115200 }
    };
    unsigned i;

    sioLocalEchoFlag = localEcho;

    for (i = 0 ; i < (sizeof(speedTable) / sizeof(speedTable[0])) ; i++) {
        if (speedTable[i].asUint == serialRate) {
            sioTtyRate = speedTable[i].asSpeed;
            break;
        }
    }
}

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
            cfsetospeed(&tio, sioTtyRate);
            cfsetispeed(&tio, sioTtyRate);
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
        if ((c == '\r') || (c == '\n')){
            int pos = *currPos;
            *currPos = 0;

            if (pos < 1) {
                /* nothing here */
                return 0;
            } else {
                msgBuff[pos++] = '\n';
                msgBuff[pos++] = '\0';
    
                if (sioLocalEchoFlag) {
                    write(fd, "\r\n", 2);
                }
    
                if (sioVerboseFlag) {
                    fprintf(stdout, "%s: buff = %s", __FUNCTION__, msgBuff);
                }
    
                return pos;
            }
        } else if (sioLocalEchoFlag && (c == '\b')) {
            /*  If it's BS with nothing in buffer, ignore, else
             *  back up stream, erasing last character typed. */
            if (*currPos > 0) {
                (*currPos)--;
                write(fd, "\b \b", 3);
            }
            return 0;
        } else {
            if (*currPos >= bufSize) {
                /* buffer full with no CR can't be good; flush it */
                *currPos = 0;
            } else {
                msgBuff[(*currPos)++] = c;
                if (sioLocalEchoFlag) {
                    write(fd, &c, 1);
                }
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
