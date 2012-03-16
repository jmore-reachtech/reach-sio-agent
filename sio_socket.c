
#include <stdio.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "sio_agent.h"



#define MAXPENDING 1

/*  Used to send to socket after accept, when called by
 *  TTY reader.
 */
int	socketFd;

static void sioReadClientConnection( int newFd );

static	void sioDieWithError(char *errorMessage)
{
    printf( "Exiting: %s\n", errorMessage );
    exit( 1 );
}

static	int sioCreateTCPServerSocket( unsigned short port )
{
int			sock;
struct	sockaddr_in	echoServAddr;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        sioDieWithError("socket() failed");
      
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET; 
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        sioDieWithError("bind() failed");

    if (listen(sock, MAXPENDING) < 0)
        sioDieWithError("listen() failed");

    return sock;
}


int sioAcceptTCPConnection( int servSock )
{
int 				clntSock;
struct 		sockaddr_in	echoClntAddr;
unsigned	int		clntLen;

    clntLen = sizeof(echoClntAddr);
    
    if ((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr,
           &clntLen)) < 0) {
	sioDieWithError("accept() failed");
    }
    
    printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

    return clntSock;
}


int sioSocketInit( int port, char *devFname )
{
int	sockFd, clientSockFd, status;

    sockFd = sioCreateTCPServerSocket( port );

    /*  This really isn't every going to accept more than one
     *  one connection, and Agent isn't designed to handle more
     *  than one connection.  So, it just waits for child to die after
     *  spawning off.
     */
    for (;;)
    {
	clientSockFd = sioAcceptTCPConnection( sockFd );

	/*  Public Socket Descriptor for interaction with QML Viewer
         */
	socketFd = clientSockFd;

	/*  Spawn a TTY Reader Thread.
         */
	sioSerialInit( devFname );

	/*  This returns if client closes connection.  Tear down
	 *  entire session, return to waiting for connection to 
	 *  come back.
	 */
	sioReadClientConnection( clientSockFd );

	if( verboseFlag )
	    fprintf( stdout, "sioSocketInit(): tearing down\n" );

	sioSerialShutdown();

    }
}


static void sioReadClientConnection( int newFd )
{
char	msgBuff[ 128 ];
int	cnt;


    for( ;; ) {

    	if( (cnt = recv( newFd, msgBuff, 128, 0 )) <= 0) {
	    printf( "sioHandleServer(): recv() failed, client closed\n" );
	    return;
	} else {

	    msgBuff[ cnt ] = 0;
	    if( verboseFlag )
	    	printf( "%s", msgBuff );

	    /*  Check for Ping message, if so, respond to it.
             */

	    if( !strncmp( "ping", msgBuff, strlen( "ping" )) ) {

		if( verboseFlag )
		    fprintf( stdout, "sioHandleServer(): sending pong!\n" );

		send( newFd, "pong!\n", strlen( "pong!\n"), 0 );

	    } else {

	        sioTTYWriter( msgBuff );

	    }
	}
    }
}


void sioSocketSendToClient( char *buff )
{
int cnt = strlen( buff );

    if( send( socketFd, buff, cnt, 0) != cnt ) {
	printf( "socket_send_to_client(): send() failed, %d\n", socketFd );
	perror( "what's messed up?" );
    }
}
