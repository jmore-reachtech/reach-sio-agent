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


static void *sioTTYReader(void *arg);

static int tty_fd;
static struct termios tio;

pthread_t sioTTYReader_thread;

int sioSerialInit(char *tty_dev)
{
    memset(&tio,0,sizeof(tio));
    tio.c_iflag=0;
    tio.c_oflag=0;
    tio.c_cflag=CS8|CREAD|CLOCAL;
    tio.c_lflag=0;
    tio.c_cc[VMIN]=1;
    tio.c_cc[VTIME]=5;

    tty_fd = open(tty_dev, O_RDWR);
    if (tty_fd < 0) {
        printf("can't open %s\n", tty_dev);
        exit(1);
    }
    cfsetospeed(&tio, B115200);
    cfsetispeed(&tio,B115200);
    tcsetattr(tty_fd,TCSANOW,&tio);

    pthread_create(&sioTTYReader_thread, NULL, sioTTYReader, NULL);

    return(0);
}

void sioSerialShutdown()
{
    if (verboseFlag) {
        fprintf(stdout, "sioSerialShutdown(): killing pthread reader\n");
    }

    pthread_cancel(sioTTYReader_thread);

    close(tty_fd);
}

static void *sioTTYReader(void *arg)
{
    char buff[200];
    int  i = 0;

    if ( verboseFlag )
        printf( "sioTTYReader(): running\n" );

    /* Make sure we can get canceled.*/
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    /*  Gather entire string, drop CR, send to socket handler. */
    for (;;) {
        if (read(tty_fd, &buff[ i ], 1) > 0) {
            if (buff[i] == '\r') {
                buff[i] = '\n';
                buff[++i] = 0;

                if (localEchoFlag) {
                    write(tty_fd, "\r\n", 2);
                }

                sioSocketSendToClient(buff);
                i = 0;

                if (verboseFlag) {
                    fprintf(stdout, "sioTTYReader(): buff = %s", buff);
                }
            } else if (buff[i] == '\n') {
                continue;
            } else if (buff[i] == '\b') {
                /*  If it's BS with nothing in buffer, ignore, else
                 *  back up stream, erasing last character typed. */
                if (i) {
                    i--;
                    write(tty_fd, "\b \b", 3);
                } else {
                    continue;
                }
            } else {
                if (localEchoFlag) {
                    write(tty_fd, &buff[i], 1);
                }

                i++;
            }
        } else {
            if (verboseFlag) {
                printf("sio_tty_reader(): error on read()\n");
            }
        }
    }

    return 0;
}


void sioTTYWriter(char *buff)
{
    char *retMsg;

    if (verboseFlag) {
        fprintf(stdout, "sioTTYWRiter(): got buff %s", buff);
    }

    /*  Check for escape sequence, intercept traffic.  Else, send out TTY. */
    if (buff[0] == '*') {
        retMsg = sioHandleLocal(buff);
        if (retMsg) {
            sioSocketSendToClient(retMsg);
            free(retMsg);
        }
    } else {
        if (write(tty_fd, buff, strlen(buff) ) < 0) {
            if (verboseFlag) {
                fprintf(stdout, "sio_tty_writter(): error on write()\n");
            }
        }

        /*  Put a CR in; we may want to revisit how to do this. */
        write(tty_fd, "\r", 1);
    }
}
