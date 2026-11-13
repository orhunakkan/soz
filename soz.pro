QT += core multimedia network
QT -= gui

CONFIG += c++17 release console
CONFIG -= app_bundle
TARGET = soz
TEMPLATE = app

# whisper.cpp is built by bin/build (or the PKGBUILD) as static libraries with
# the Vulkan backend, so the binary carries the engine and needs no extra .so.
isEmpty(WHISPER_SRC): WHISPER_SRC = $$PWD/third_party/whisper.cpp-1.9.5
isEmpty(WHISPER_BUILD): WHISPER_BUILD = $$OUT_PWD/whisper

INCLUDEPATH += $$WHISPER_SRC/include $$WHISPER_SRC/ggml/include

# The ggml libraries refer to each other both ways, so link them as a group.
LIBS += \
    -Wl,--start-group \
    $$WHISPER_BUILD/src/libwhisper.a \
    $$WHISPER_BUILD/ggml/src/ggml-vulkan/libggml-vulkan.a \
    $$WHISPER_BUILD/ggml/src/libggml-cpu.a \
    $$WHISPER_BUILD/ggml/src/libggml.a \
    $$WHISPER_BUILD/ggml/src/libggml-base.a \
    -Wl,--end-group \
    -lvulkan -fopenmp -ldl -lpthread

HEADERS += \
    src/bar.h \
    src/client.h \
    src/cues.h \
    src/daemon.h \
    src/engine.h \
    src/inserter.h \
    src/paths.h \
    src/recorder.h \
    src/switcher.h \
    src/text.h

SOURCES += \
    src/main.cpp \
    src/bar.cpp \
    src/client.cpp \
    src/cues.cpp \
    src/daemon.cpp \
    src/engine.cpp \
    src/inserter.cpp \
    src/paths.cpp \
    src/recorder.cpp \
    src/switcher.cpp \
    src/text.cpp
