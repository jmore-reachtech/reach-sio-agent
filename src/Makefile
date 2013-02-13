sources = sio_agent.c \
	sio_local.c \
	sio_serial.c \
	sio_socket.c \
	logmsg.c

headers = sio_agent.h

LDFLAGS=-pthread

CFLAGS=-Wall

ifeq ($(DEBUG_DEF),1)
	DEBUG = -g -O0 -DDEBUG
else
	DEBUG = -O2
endif

all: sio-agent

sio-agent: $(sources) $(headers)
	$(CC) -DSIO_VERSION='"$(AGENT_VERSION)"' $(CFLAGS) $(LDFLAGS) $(DEBUG) -o $@ $(sources)

clean:
	$(RM) sio-agent

.PHONY: all clean
