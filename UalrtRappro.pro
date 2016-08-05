#-------------------------------------------------
#
# Project created by QtCreator 2016-07-31T17:59:54
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
TARGET = UalrtRappro
TEMPLATE = app
TRANSLATIONS = rappro_fr.ts

SOURCES += main.cpp\
        mainwindow.cpp \
    ofxparser.cpp \
    statementtablemodel.cpp \
    logindialog.cpp

HEADERS  += mainwindow.h \
    ofxparser.h \
    statementtablemodel.h \
    logindialog.h

FORMS    += mainwindow.ui \
    logindialog.ui

RESOURCES += \
    resources.qrc

DISTFILES += \
    schema.sql
