#ifndef SIO_AGENT_H
#define SIO_AGENT_H

/* functions defined in sio_socket.c */
int sioSocketInit(unsigned short port, int *addressFamily,
    const char *unixSocketPath);
int sioAcceptConnection(int serverFd, int addressFamily);
int sioSocketRead(int newFd, char *msgBuff, size_t bufferSize);
void sioSocketWrite(int socketFd, const char *buff);

/* functions defined in sio_socket.c */
int sioTTYInit(const char *tty_dev);
int sioTTYRead(int fd, char *msgBuff, size_t bufSize, off_t *currPos);
void sioTTYWrite(int serialFd, const char *msgBuff, int buffSize);

/* functions defined in sio_local.c */
char *sioHandleLocal(char *qmlString);

int verboseFlag;
int localEchoFlag;

#define SIO_DEFAULT_AGENT_PORT 7880
#define SIO_AGENT_UNIX_SOCKET "/tmp/sioSocket"
#define SIO_DEFAULT_SERIAL_DEVICE "/dev/ttyUSB0"

#endif  /* SIO_AGENT_H */
