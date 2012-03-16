CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-lpthread

SOURCES=sio_socket.c sio_local.c sio_agent.c sio_serial.c

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=sio-agent

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o: $(CC) $(CFLAGS) $(DEBUG) $< -o $@

clean:
	rm *.o sio-agent

