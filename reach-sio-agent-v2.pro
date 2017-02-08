TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

VERSION = 2.0.1
# add #define for the version
DEFINES += SIO_VERSION=\\\"$$VERSION\\\"

TARGET = sio-agent
target.path=/application/bin
INSTALLS += target

QMAKE_CFLAGS_DEBUG =  -O0 -pipe -g -feliminate-unused-debug-types

SOURCES += src/sio_agent.c \
        src/sio_local.c \
        src/sio_serial.c \
        src/sio_socket.c \
        src/logmsg.c

HEADERS += src/sio_agent.h

