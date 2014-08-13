#ifndef SIO_AGENT_H
#define SIO_AGENT_H

#include <syslog.h>
#include <sys/stat.h>

/* types */
struct FdPair {
    int inFd;
    int outFd;
    int maxFd;
};

/* functions defined in sio_socket.c */
int sioTioSocketInit(unsigned short port, int *addressFamily,
    const char *unixSocketPath);
int sioTioSocketAccept(int serverFd, int addressFamily);
int sioTioSocketRead(int newFd, char *msgBuff, size_t bufferSize);
void sioTioSocketWrite(int socketFd, const char *buff);

/* functions in sio_serial.c */
void sioTtySetParams(int localEcho, unsigned int serialRate, int enableRS485);
int sioTtyInit(const char *tty_dev);
int sioTtyRead(int fd, char *msgBuff, size_t bufSize, off_t *currPos);
void sioTtyWrite(int serialFd, const char *msgBuff, int buffSize);

/* functions defined in sio_local.c */
char *sioHandleLocal(char *qmlString);

/* functions exported from logmsg.c */
void LogOpen(const char *ident, int logToSyslog, const char *logFilePath,
    int verboseFlag);
void LogMsg(int level, const char *fmt, ...);

#define SIO_DEFAULT_AGENT_PORT 7880
#define SIO_AGENT_UNIX_SOCKET "/tmp/sioSocket"
#define SIO_DEFAULT_SERIAL_DEVICE "/dev/ttyUSB0"
#define SIO_DEFAULT_SERIAL_RATE 115200

#define SIO_BUFFER_SIZE 256

#endif  /* SIO_AGENT_H */
