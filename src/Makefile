sources = sio_agent.c \
	sio_agent.h \
	sio_local.c \
	sio_serial.c \
	sio_socket.c \

LDFLAGS=-lpthread

all: sio-agent

sio-agent: $(sources)
	$(CC) $(LDFLAGS) -g -O0 -o $@ $(sources)

clean:
	-rm sio-agent

.PHONY: all clean
