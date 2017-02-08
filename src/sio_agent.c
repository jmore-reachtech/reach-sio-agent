#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sio_agent.h"

/* module-wide "global" variables */
static int keepGoing;
static const char *progName;

static void sioDumpHelp();
static void sioAgent(const char *serialName, int useStdio,
    unsigned short tcpPort);

static inline int max(int a, int b) { return (a > b) ? a : b; }

int main(int argc, char *argv[])
{
    int daemonFlag          = 0;
    int localEcho           = 0;
    const char *serialName  = SIO_DEFAULT_SERIAL_DEVICE;
    unsigned short tcpPort  = 0;
    unsigned int baudRate   = SIO_DEFAULT_SERIAL_RATE;
    int useStdio            = 0;
    int enableRS485         = 0;
    const char *logFilePath = 0;
    /* 
     * syslog isn't installed on the target so it's disabled in this program
     * by requiring an argument to -o|--log.
     */ 
    int logToSyslog = 0;
    int verboseFlag = 0;

    /* allocate memory for progName since basename() modifies it */
    const size_t nameLen = strlen(argv[0]) + 1;
    char arg0[nameLen];
    memcpy(arg0, argv[0], nameLen);
    progName = basename(arg0);

    /* override the TTY from environment if variable is set */
    char *sioTTYVar = getenv("SIO_AGENT_TTY");
    if (sioTTYVar) {
        serialName = sioTTYVar;
        fprintf(stderr, "sio_agent: Setting device to %s from SIO_AGENT_TTY\n",
            serialName);
    }

    while (1) {
        static struct option longOptions[] = {
            { "baud",       required_argument, 0, 'b' },
            { "daemon",     no_argument,       0, 'd' },
            { "test",       no_argument,       0, 'e' },
            { "pty",        no_argument,       0, 'p' },
            { "serial",     required_argument, 0, 't' },
            { "tcp-port",   required_argument, 0, 's' },
            { "rs485",      optional_argument, 0, 'f' },
            { "stdio",      no_argument,       0, 'i' },
            { "verbose",    no_argument,       0, 'v' },
            { "help",       no_argument,       0, 'h' },
            { 0,            0, 0,  0  }
        };
        int c = getopt_long(argc, argv, "b:s:dilpf::t:vh?", longOptions, 0);

        if (c == -1) {
            break;  // no more options to process
        }

        switch (c) {
        case 'b':
            baudRate = atoi(optarg);
            break;

        case 'd':
            daemonFlag = 1;
            break;

        case 'i':
            useStdio = 1;
            break;

        case 'e':
            localEcho = 1;
            break;

        case 'p':
            serialName = 0;  /* special value indicates pty */
            break;

        case 's':
            tcpPort = atoi(optarg);
            break;

        case 't':
            serialName = optarg;
            break;
        
        case 'f':
            enableRS485 = 1;
            break;

        case 'v':
            verboseFlag = 1;
            break;

        case '?':
        case 'h':
        default:
            sioDumpHelp();
            exit(1);
        }
    }

    /* set up logging to syslog or file; will be STDERR not told otherwise */
    LogOpen(progName, logToSyslog, logFilePath, verboseFlag);

    /*  Keep STDIO going for now.
     */
    if (daemonFlag) {
        daemon(0, 1);
        useStdio = 0;  /* don't use stdin and stdout if daemon */
    }

    if (useStdio) {
        localEcho = 0;  /* terminal should already do this */
    }

    if (tcpPort == 0)
        tcpPort = SIO_DEFAULT_AGENT_PORT;

    sioTtySetParams(localEcho, baudRate, enableRS485);
    sioAgent(serialName, useStdio, tcpPort);

    return 0;
}

static void sioDumpHelp()
{
    fprintf(stderr, "SIO Agent %s \n\n",SIO_VERSION);
	
    fprintf(stderr, "usage: %s [options]\n"
        "  where options are:\n"
        "    -b<rate>   | --baud=<rate>       serial port bit rate, default = %d\n"
        "    -d         | --daemon            run in background\n"
        "    -e         | --test              echo, backspace\n"
        "    -i         | --stdio             use standard I/O instead of serial\n"
        "    -p         | --pty               use pty device instead of real serial\n"
        "    -s[<port>] | --tcp_port[=<port>] change TCP socket, default = %d\n"
        "    -t         | --serial <dev>      use <dev> instead of /dev/ttyUSB0\n"
        "    -f         | --rs485             enable RS-485 mode\n"
        "    -v         | --verbose           print progress messages\n"
        "    -h         | -? | --help         print usage information\n",
        progName, SIO_DEFAULT_SERIAL_RATE, SIO_DEFAULT_AGENT_PORT);
}

static void sioInterruptHandler()
{
    keepGoing = 0;
}

/**
 * This is the main loop function.  It opens and configures the 
 * serial port (or pty) and opens the socket TCP
 * and enters a select loop waiting for connections.
 * 
 * @param serialName the name of the serial device to open or 0 
 *                   to use a pty
 * @param useStdio non-zero indicates that stdin and stdout should be used for 
 *                 instead of a serial device
 * @param tcpPort the port number to open to connect to the qml-viewer socket.
 *
 */
static void sioAgent(const char *serialName, int useStdio,
    unsigned short tcpPort)
{
    fd_set currFdSet;

    /* install a signal handler to close the socket */
    struct sigaction a;
    memset(&a, 0, sizeof(a));
    a.sa_handler = sioInterruptHandler;
    if (sigaction(SIGINT, &a, 0) != 0) {
        LogMsg(LOG_ERR, "[SIO] sigaction() failed, errno = %d\n", errno);
        exit(1);
    }

    if (sigaction(SIGTERM, &a, 0) != 0) {
        LogMsg(LOG_ERR, "[SIO] sigaction() failed, errno = %d\n", errno);
        exit(1);
    }

    /* connect socket */
    int qmlFd = sioQMLConnect(tcpPort);
    FD_ZERO(&currFdSet);
    FD_SET(qmlFd, &currFdSet);

    /* execution remains in this loop until a fatal error, SIGINT or SIGTERM */
    keepGoing = 1;

    while (keepGoing) {
        int nfds = 0;
        off_t serialPos = 0;
        char ttyBuff[SIO_BUFFER_SIZE];
        struct FdPair serialFds;

        if (useStdio) {
            serialFds.inFd = fileno(stdin);
            serialFds.outFd = fileno(stdout);
            serialFds.maxFd = max(serialFds.inFd, serialFds.outFd);
        } else {
            /* try opening the serial device */
            serialFds.inFd = sioTtyInit(serialName);
            if (serialFds.inFd < 0) {
                /* open failed, can't continue */
                LogMsg(LOG_ERR, "[SIO] could not open serial port %s\n", serialName);
                break;
            } else {
                serialFds.outFd = serialFds.maxFd = serialFds.inFd;
            }
        }

        FD_SET(serialFds.inFd, &currFdSet);
        if (serialFds.inFd != serialFds.outFd) {
            FD_SET(serialFds.outFd, &currFdSet);
        }
        nfds = max(serialFds.maxFd, qmlFd) + 1;

        /* 
         * This is the select loop which waits for characters to be received on 
         * the serial/pty descriptor and on either the listen socket (meaning 
         * an incoming connection is queued) or on a connected socket 
         * descriptor.
         */
        while (1) {

            if (qmlFd < 0)
                break;

            fd_set readFdSet = currFdSet;
            /* wait indefinitely for someone to blink */
            const int sel = select(nfds, &readFdSet, 0, 0, 0);

            if (sel == -1) {
                if (errno == EINTR) {
                    break;  /* drop out of inner while */
                } else {
                    LogMsg(LOG_ERR, "[SIO] select() returned -1, errno = %d\n", errno);
                    exit(1);
                }
            } else if (sel <= 0) {
                continue;
            }

            /* check for packet received on the client socket */
            if (FD_ISSET(qmlFd, &readFdSet)) {
                /* qml-viewer has something to relay to serial port */
                char msgBuff[SIO_BUFFER_SIZE];
                const int readCount = sioQMLSocketRead(qmlFd, msgBuff,
                    sizeof(msgBuff));
                if (readCount < 0) {
                    FD_CLR(qmlFd, &currFdSet);
                    qmlFd = -1;
                } else if (readCount > 0) {
                    sioTtyWrite(serialFds.outFd, msgBuff, readCount);
                }
            }

            /* check for a character on the serial port */
            if (FD_ISSET(serialFds.inFd, &readFdSet)) {
                /* 
                 * serial port has something to send to the tio_agent,
                 * if connected 
                 */
                int serialRet = sioTtyRead(serialFds.inFd, ttyBuff,
                    sizeof(ttyBuff), &serialPos);
                if (serialRet < 0) {
                    /* fall out of this loop to reopen serial port or pts */
                    break;
                } else if ((serialRet > 0) && (qmlFd >= 0)) {
                    sioQMLSocketWrite(qmlFd, ttyBuff);
                }
            }
        }

        if (useStdio) {
            /* don't try to reopen stdin/stdout */
            keepGoing = 0;
        } else {
            close(serialFds.inFd);
            FD_CLR(serialFds.inFd, &currFdSet);
        }
    }

    LogMsg(LOG_INFO, "[SIO] cleaning up\n");

    if (qmlFd >= 0) {
        close(qmlFd);
    }

}
