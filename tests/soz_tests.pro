QT += core testlib
QT -= gui
CONFIG += c++17 testcase console
TARGET = soz_tests
TEMPLATE = app

INCLUDEPATH += ../src
HEADERS += ../src/text.h ../src/paths.h ../src/bar.h
SOURCES += soz_tests.cpp ../src/text.cpp ../src/paths.cpp ../src/bar.cpp
