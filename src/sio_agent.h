#ifndef SIO_AGENT_H
#define SIO_AGENT_H

/* functions defined in sio_socket.c */
int sioTioSocketInit(unsigned short port, int *addressFamily,
    const char *unixSocketPath);
int sioTioSocketAccept(int serverFd, int addressFamily);
int sioTioSocketRead(int newFd, char *msgBuff, size_t bufferSize);
void sioTioSocketWrite(int socketFd, const char *buff);

/* functions defined in sio_socket.c */
int sioTtyInit(const char *tty_dev);
int sioTtyRead(int fd, char *msgBuff, size_t bufSize, off_t *currPos);
void sioTtyWrite(int serialFd, const char *msgBuff, int buffSize);

/* functions defined in sio_local.c */
char *sioHandleLocal(char *qmlString);

#define SIO_DEFAULT_AGENT_PORT 7880
#define SIO_AGENT_UNIX_SOCKET "/tmp/sioSocket"
#define SIO_DEFAULT_SERIAL_DEVICE "/dev/ttyUSB0"

/* global variables shared across modules */
int sioVerboseFlag;
int sioLocalEchoFlag;

#endif  /* SIO_AGENT_H */
