TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lftdi


SOURCES += \
    flash.c \
    flash_over_jtag.c \
    jtag.c \
    srec.c

HEADERS += \
    exit_codes.h \
    flash.h \
    flash_over_jtag.h \
    hw_access.h \
    jtag.h \
    srec.h
