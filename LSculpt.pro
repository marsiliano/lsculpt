# -------------------------------------------------
# Project created by QtCreator 2010-04-22T21:11:11
# -------------------------------------------------
#
# NOTE: CMakeLists.txt (repo root) is the primary, tested cross-platform
# build (Qt6, engine libs built via ExternalProject_Add). This .pro file is
# kept for Qt Creator/qmake users but is not the build this migration was
# verified against beyond removing the dead LDVLib reference below and
# adding the new sources.
QT += widgets \
    core
contains(QT_VERSION, ^6\\..*) {
    QT += openglwidgets
}
TARGET = LSculpt
TEMPLATE = app

# Comment this out to build UI version of LSculpt
# CONFIG += console

console:DEFINES += LSCULPT_CONSOLE
# QMAKE_LFLAGS += -static-libgcc
SOURCES += LSculpt.cpp \
    rply.c \
    obj_parser.cpp
HEADERS += rply.h \
    LSculpt_functions.h \
    LSculpt_classes.h \
    smVector.h \
    obj_parser.hpp \
    obj.hpp \
    obj_config.hpp \
    LSculpt_obj.h
!console {
    INCLUDEPATH += $$PWD/ldview
    LIBS += -L$$PWD/ldview/LDLib -lLDraw-qt$$QT_MAJOR_VERSION \
            -L$$PWD/ldview/TRE -lTRE-qt$$QT_MAJOR_VERSION \
            -L$$PWD/ldview/LDLoader -lLDLoader-qt$$QT_MAJOR_VERSION \
            -L$$PWD/ldview/LDExporter -lLDExporter-qt$$QT_MAJOR_VERSION \
            -L$$PWD/ldview/TCFoundation -lTCFoundation-qt$$QT_MAJOR_VERSION \
            -L$$PWD/ldview/3rdParty/lib3ds -l3ds \
            -lgl2ps -ltinyxml2 -lminizip
    unix:!macx: LIBS += -lX11
    # The five libraries above have no qmake/.pro target of their own --
    # see CMakeLists.txt's comment block for why (GNU-Makefile-only,
    # requires USE_CPP17=YES + a Qt framework include path on macOS). Build
    # them with the same invocation CMakeLists.txt uses before building this
    # target with qmake.
    SOURCES += main.cpp \
        lsculptmainwin.cpp \
        argpanel.cpp \
        aboutdialog.cpp \
        ldrawpreviewwidget.cpp
    HEADERS += lsculptmainwin.h \
        argpanel.h \
        aboutdialog.h \
        ldrawpreviewwidget.h
    FORMS += lsculptmainwin.ui \
        argpanel.ui \
        aboutdialog.ui \
        advanced_widgets.ui
    win32: RC_FILE = LSculpt.rc
    RESOURCES += LSculpt.qrc
}

