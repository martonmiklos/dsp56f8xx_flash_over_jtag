TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    Jtag.c \
    flash.c \
    flash_over_jtag.c \
    port_io.c \
    report.c \
    srec.c

HEADERS += \
    exit_codes.h \
    flash.h \
    flash_over_jtag.h \
    hw_access.h \
    jtag.h \
    port_io.h \
    report.h \
    srec.h
