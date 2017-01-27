TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

VERSION = 1.0.9
# add #define for the version
DEFINES += SIO_VERSION=\\\"$$VERSION\\\"

TARGET=sio-agent

SOURCES += src/sio_agent.c \
        src/sio_local.c \
        src/sio_serial.c \
        src/sio_socket.c \
        src/logmsg.c

HEADERS += src/sio_agent.h

