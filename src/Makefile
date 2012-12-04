sources = sio_agent.c \
	sio_local.c \
	sio_serial.c \
	sio_socket.c \
	logmsg.c

headers = sio_agent.h

LDFLAGS=-pthread
CFLAGS=-Wall

all: sio-agent

sio-agent: $(sources) $(headers)
	$(CC) $(CFLAGS) $(LDFLAGS) -g -O0 -o $@ $(sources)

clean:
	$(RM) sio-agent

.PHONY: all clean
