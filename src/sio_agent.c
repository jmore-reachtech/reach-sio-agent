#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include "sio_agent.h"

static void sioAgentLaunch();

extern char *optarg;
extern int opterr, optopt, optind;

int verboseFlag;
int localEchoFlag;
char devFname[200];
int daemonFlag;

static void sioDumpHelp();

int main(int argc, char *argv[])
{
    int c;
    char *sioTTYVar;

    verboseFlag = 0;
    localEchoFlag = 0;
    daemonFlag = 0;

    sioTTYVar = getenv("SIO_AGENT_TTY");
    if (sioTTYVar) {
        fprintf(stderr, "sio_agent: Setting device to %s from SIO_AGENT_TTY\n",
            sioTTYVar);
        fprintf(stderr, "Use -t devname to override\n");

        strcpy(devFname, sioTTYVar);
    } else {
        fprintf(stderr, "sio_agent: Setting device to default /dev/ttyUSB0\n");
        fprintf(stderr, "Use -t devname to override\n" );

        strcpy(devFname, "/dev/ttyUSB0");
    }

    /*  Options:
     *
     *   -d            # Run as daemon in background
     *   -t dev        # Override /dev/ttyUSB0
     *   -l            # Local echo on
     *   -v            # Verbose
     *   -h|?          # Dump Help
     */
    opterr = 0;
    while ((c = getopt( argc, argv, "lvdt:h?" )) != EOF) {
        switch (c) {

        case 't':
            strcpy( devFname, optarg );
            break;

        case 'v':
            verboseFlag = 1;
            break;

        case 'l':
            localEchoFlag = 1;
            break;

        case 'd':
            daemonFlag = 1;
            break;

        case '?':
        case 'h':
        default:
            sioDumpHelp();
            exit(1);
        }
    }

    sioAgentLaunch();

    return 0;
}

static  const char *sioHelpStrings[] = {
    " ",
    "usage: sio_agent [options]",
    " ",
    "-d            # Run as daemon in background",
    "-t dev        # Override /dev/ttyUSB0",
    "-l            # Local echo on",
    "-v            # Verbose",
    "-h|?          # Dump Help",
    ""
};

static void sioDumpHelp()
{
    int i = 0;

    while (sioHelpStrings[ i ]) {
        fprintf( stderr, "%s\n", sioHelpStrings[ i++ ] );
    }
}

static  void sioAgentLaunch()
{
    /*  Keep STDIO going for now.
     */
    if (daemonFlag) {
        daemon(0, 1);
    }

    /*  Should never return; just wait for connections and process.
     */
    sioSocketInit(SIO_AGENT_PORT, devFname);
}
