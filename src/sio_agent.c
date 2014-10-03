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
    unsigned short tcpPort, const char *unixSocketPath);
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
            { "log",        required_argument, 0, 'o' },
            { "pty",        no_argument,       0, 'p' },
            { "serial",     required_argument, 0, 't' },
            { "sio-port",   optional_argument, 0, 's' },
            { "rs485",      optional_argument, 0, 'f' },
            { "stdio",      no_argument,       0, 'i' },
            { "verbose",    no_argument,       0, 'v' },
            { "help",       no_argument,       0, 'h' },
            { 0,            0, 0,  0  }
        };
        int c = getopt_long(argc, argv, "b:dilo:psf::t:vh?", longOptions, 0);

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

        case 'o':
            if (optarg == 0) {
                logToSyslog = 1;
                logFilePath = 0;
            } else {
                logToSyslog = 0;
                logFilePath = optarg;
            }
            break;

        case 'p':
            serialName = 0;  /* special value indicates pty */
            break;

        case 's':
            tcpPort = (optarg == 0) ? SIO_DEFAULT_AGENT_PORT : atoi(optarg);
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

    sioTtySetParams(localEcho, baudRate, enableRS485);
    sioAgent(serialName, useStdio, tcpPort, SIO_AGENT_UNIX_SOCKET);

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
        "    -o<path>   | --logfile=<path>    log to file instead of stderr\n"
        "    -p         | --pty               use pty device instead of real serial\n"
        "    -s[<port>] | --sio_port[=<port>] use TCP socket, default = %d\n"
        "    -t         | --serial <dev>      use <dev> instead of /dev/ttyUSB0\n"
        "    -f         | --rs485             enable RS-485 mode\n"
        "    -v         | --verbose           print progress messages\n"
        "    -h         | -? | --help         print usage information\n",
        progName, SIO_DEFAULT_SERIAL_RATE, SIO_DEFAULT_AGENT_PORT);
}

static void sioInterruptHandler(int sig)
{
    keepGoing = 0;
}

/**
 * This is the main loop function.  It opens and configures the 
 * serial port (or pty) and opens the socket (TCP or Unix 
 * domain) and enters a select loop waiting for connections. 
 * 
 * @param serialName the name of the serial device to open or 0 
 *                   to use a pty
 * @param useStdio non-zero indicates that stdin and stdout should be used for 
 *                 instead of a serial device
 * @param tcpPort if non-zero, the port number to open for 
 *                accepting connections from the tio-agent; if
 *                zero, uses a hard-coded named Unix domain
 *                socket
 * @param unixSocketPath the file system path to use for a Unix domain socket; 
 *                       0 means use TCP
 */
static void sioAgent(const char *serialName, int useStdio,
    unsigned short tcpPort, const char *unixSocketPath)
{
    fd_set currFdSet;
    int connectedFd = -1;  /* not currently connected */

    {
        /* install a signal handler to remove the socket file */
        struct sigaction a;
        memset(&a, 0, sizeof(a));
        a.sa_handler = sioInterruptHandler;
        if (sigaction(SIGINT, &a, 0) != 0) {
            LogMsg(LOG_ERR, "sigaction() failed, errno = %d\n", errno);
            exit(1);
        }
    }

    /* open the server socket */
    int addressFamily = 0;
    const int listenFd = sioTioSocketInit(tcpPort, &addressFamily,
        unixSocketPath);
    if (listenFd < 0) {
        /* open failed, can't continue */
        LogMsg(LOG_ERR, "could not open server socket\n");
        return;
    }

    FD_ZERO(&currFdSet);
    FD_SET(listenFd, &currFdSet);

    /* execution remains in this loop until a fatal error or SIGINT */
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
                LogMsg(LOG_ERR, "could not open serial port %s\n", serialName);
                break;
            } else {
                serialFds.outFd = serialFds.maxFd = serialFds.inFd;
            }
        }

        FD_SET(serialFds.inFd, &currFdSet);
        if (serialFds.inFd != serialFds.outFd) {
            FD_SET(serialFds.outFd, &currFdSet);
        }
        nfds = max(serialFds.maxFd,
            (connectedFd >= 0) ? connectedFd : listenFd) + 1;

        /* 
         * This is the select loop which waits for characters to be received on 
         * the serial/pty descriptor and on either the listen socket (meaning 
         * an incoming connection is queued) or on a connected socket 
         * descriptor.
         */
        while (1) {
            /* wait indefinitely for someone to blink */
            fd_set readFdSet = currFdSet;
            const int sel = select(nfds, &readFdSet, 0, 0, 0);

            if (sel == -1) {
                if (errno == EINTR) {
                    break;  /* drop out of inner while */
                } else {
                    LogMsg(LOG_ERR, "select() returned -1, errno = %d\n", errno);
                    exit(1);
                }
            } else if (sel <= 0) {
                continue;
            }

            /* check for a new connection to accept */
            if (FD_ISSET(listenFd, &readFdSet)) {
                /* new connection is here, accept it */
                connectedFd = sioTioSocketAccept(listenFd, addressFamily);
                if (connectedFd >= 0) {
                    FD_CLR(listenFd, &currFdSet);
                    FD_SET(connectedFd, &currFdSet);
                    nfds = max(serialFds.maxFd, connectedFd) + 1;
                }
            }

            /* check for packet received on the client socket */
            if ((connectedFd >= 0) && FD_ISSET(connectedFd, &readFdSet)) {
                /* connected tio_agent has something to relay to serial port */
                char msgBuff[SIO_BUFFER_SIZE];
                const int readCount = sioTioSocketRead(connectedFd, msgBuff,
                    sizeof(msgBuff));
                if (readCount < 0) {
                    FD_CLR(connectedFd, &currFdSet);
                    FD_SET(listenFd, &currFdSet);
                    nfds = max(serialFds.maxFd, listenFd) + 1;
                    connectedFd = -1;
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
                } else if ((serialRet > 0) && (connectedFd >= 0)) {
                    sioTioSocketWrite(connectedFd, ttyBuff);
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

    LogMsg(LOG_INFO, "cleaning up\n");

    if (connectedFd >= 0) {
        close(connectedFd);
    }
    if (listenFd >= 0) {
        close(listenFd);
    }

    if (tcpPort == 0) {
        /* best effort removal of socket */
        const int rv = unlink(unixSocketPath);
        if (rv == 0) {
            LogMsg(LOG_INFO, "socket file %s unlinked\n", unixSocketPath);
        } else {
            LogMsg(LOG_INFO, "socket file %s unlink failed\n", unixSocketPath);
        }
    }
}
