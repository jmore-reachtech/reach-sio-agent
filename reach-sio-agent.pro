TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += src/sio_agent.c \
        src/sio_local.c \
        src/sio_serial.c \
        src/sio_socket.c \
        src/logmsg.c

HEADERS += src/sio_agent.h

