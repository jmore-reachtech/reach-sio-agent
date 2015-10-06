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
#include <sys/stat.h>

#include "sio_agent.h"
# define CRTSCTS  020000000000
#define BAUDRATE B230400

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
    tio.c_cflag = BAUDRATE | CS8 | CREAD | CLOCAL;
    tio.c_iflag = IGNPAR | ICRNL;
    tio.c_oflag = 0;
    tio.c_lflag = ICANON;
    tio.c_cc[VINTR]    = 0;     /* Ctrl-c */
    tio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
    tio.c_cc[VERASE]   = 0;     /* del */
    tio.c_cc[VKILL]    = 0;     /* @ */
    tio.c_cc[VEOF]     = 4;     /* Ctrl-d */
    tio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
    tio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
    tio.c_cc[VSWTC]    = 0;     /* '\0' */
    tio.c_cc[VSTART]   = 0;     /* Ctrl-q */
    tio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
    tio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
    tio.c_cc[VEOL]     = 0;     /* '\0' */
    tio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
    tio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
    tio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
    tio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
    tio.c_cc[VEOL2]    = 0;     /* '\0' */

    if (tty_dev == 0) {
        fd = posix_openpt(O_RDWR | O_NOCTTY);
        if (fd < 0) {
            LogMsg(LOG_ERR, "[SIO] can't open %s\n", tty_dev);
        } else {
            tcsetattr(fd, TCSANOW, &tio);
            grantpt(fd);
            unlockpt(fd);
            LogMsg(LOG_NOTICE, "[SIO] slave port = %s\n", ptsname(fd));
        }
    } else {
        fd = open(tty_dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) {
            LogMsg(LOG_ERR, "[SIO] can't open %s\n", tty_dev);
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
                    LogMsg(LOG_ERR,"Error: TIOCSRS485 ioctl not supported.\n");
                }
            }
            cfsetospeed(&tio, sioTtyRate);
            cfsetispeed(&tio, sioTtyRate);
            LogMsg(LOG_INFO,"Port settings set.\n");
            tcsetattr(fd, TCSANOW, &tio);
        }
    }

    return fd;
}

int sioTtyRead(int fd, char *msgBuff, size_t bufSize, off_t *currPos)
{
    return read(fd, msgBuff, bufSize);
}


void sioTtyWrite(int serialFd, const char *msgBuff, int buffSize)
{
    LogMsg(LOG_INFO, "[SIO] sending => \"%s\"\n", msgBuff);


    if (write(serialFd, msgBuff, buffSize) < 0) {
        LogMsg(LOG_INFO, "[SIO] %s(): error on write()\n", __FUNCTION__);
    }
}

