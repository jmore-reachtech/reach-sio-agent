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
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <asm-generic/ioctls.h> 

#include "sio_agent.h"

static int sioLocalEchoFlag;
static speed_t sioTtyRate;
static int rs485_mode;

void sioTtySetParams(int localEcho, unsigned int serialRate, int enable_rs485)
{
    static const struct {
        unsigned int asUint; speed_t asSpeed;
    } speedTable[] = {
        {   1200,   B1200 },
        {   9600,   B9600 },
        {  19200,  B19200 },
        {  38400,  B38400 },
        {  57600,  B57600 },
        { 115200, B115200 },
		{ 230400, B230400 }
    };
    unsigned i;

    sioLocalEchoFlag = localEcho;
    rs485_mode = enable_rs485;

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
    struct serial_rs485 rs485conf;
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
            LogMsg(LOG_ERR, "can't open %s\n", tty_dev);
        } else {
            tcsetattr(fd, TCSANOW, &tio);
            grantpt(fd);
            unlockpt(fd);
            LogMsg(LOG_NOTICE, "slave port = %s\n", ptsname(fd));
        }
    } else {
        fd = open(tty_dev, O_RDWR);
        if (fd < 0) {
            LogMsg(LOG_ERR, "can't open %s\n", tty_dev);
        } else {
            if (rs485_mode) {
                /* Enable RS-485 mode: */
                rs485conf.flags |= SER_RS485_ENABLED;
 
                /* Set rts/txen delay before send, if needed: (in microseconds) */
                rs485conf.delay_rts_before_send = 0;
 
                /* Set rts/txen delay after send, if needed: (in microseconds) */
                rs485conf.delay_rts_after_send = 0;
 
                /* Write the current state of the RS-485 options with ioctl. */
                if (ioctl (fd, TIOCSRS485, &rs485conf) < 0) {
                    printf("Error: TIOCSRS485 ioctl not supported.\n");
                }
            }
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

    for(;;) {
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

                    LogMsg(LOG_INFO, "%s: buff = %s", __FUNCTION__, msgBuff);

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
            LogMsg(LOG_INFO, "sio_tty_reader(): error on read()\n");
            return -1;
        }
    }
}


void sioTtyWrite(int serialFd, const char *msgBuff, int buffSize)
{
    LogMsg(LOG_INFO, "sioTTYWRiter(): got buff %s", msgBuff);


    if (write(serialFd, msgBuff, buffSize) < 0) {
        LogMsg(LOG_INFO, "%s(): error on write()\n", __FUNCTION__);
    }
}

