QT       += core gui network widgets sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 编译生成的可执行文件名（Windows 下为 AGTProject.exe）
TARGET = AGTProject

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    tcpclient.cpp \
    dbworker.cpp \
    dataprocessworker.cpp

HEADERS += \
    tcpclient.h \
    dbworker.h \
    dataprocessworker.h

FORMS += \
    tcpclient.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
