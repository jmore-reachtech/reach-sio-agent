#ifndef	SIO_AGENT_H
#define	SIO_AGENT_H

int sioSerialInit( char *tty_dev );
void sioTTYWriter( char *buff );
void sioSerialShutdown();

int sioSocketInit( int port, char *devName );
void sioSocketSendToClient( char *buff );

char *sioHandleLocal( char *qmlString );

int     verboseFlag;
int     localEchoFlag;
char    devFname[ 200 ];


#define	SIO_AGENT_PORT	7880

#endif
