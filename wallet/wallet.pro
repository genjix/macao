######################################################################
# Automatically generated by qmake (2.01a) Thu Sep 12 12:24:33 2013
######################################################################

TEMPLATE = app
TARGET = wallet
QT += core gui
HEADERS += beginwindow.h
SOURCES += main.cpp beginwindow.cpp
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libbitcoin libobelisk
}

# Directories
