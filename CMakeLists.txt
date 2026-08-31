cmake_minimum_required(VERSION 3.16)
project(LSculpt CXX C)

# --- Options -----------------------------------------------------------
option(LSCULPT_CONSOLE "Build the console-only mesh/sculpt core, no Qt GUI or LDView preview" OFF)

set(LDVIEW_DIR ${CMAKE_SOURCE_DIR}/ldview)

# --- Console-only core target (no LDView dependency, but still uses
# QFileInfo/QString from QtCore -- LSculpt.pro's `QT += widgets core` is
# unconditional, only the LDVLib link and GUI-only sources are gated on
# `!console`) ------------------------------------------------------------
# Mirrors LSculpt.pro's `CONFIG += console` mode: mesh/sculpt logic only.
# Useful as a preview-free fallback and as the first cross-platform smoke
# test, independent of the Qt GUI / LDView viewer work below.
find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

add_executable(LSculptConsole
	LSculpt.cpp
	rply.c
	obj_parser.cpp
)
target_compile_definitions(LSculptConsole PRIVATE LSCULPT_CONSOLE)
target_link_libraries(LSculptConsole Qt${QT_VERSION_MAJOR}::Core)
set_target_properties(LSculptConsole PROPERTIES OUTPUT_NAME "LSculpt")

if(LSCULPT_CONSOLE)
	return()
endif()

# --- Everything below here is the full GUI build ------------------------

set(OpenGL_GL_PREFERENCE LEGACY)

find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets OpenGL OpenGLWidgets REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)
if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
endif()

message(STATUS "Qt:   ${QT_VERSION}")
message(STATUS "PNG:  ${PNG_VERSION_STRING}")
message(STATUS "JPEG: ${JPEG_VERSION}")
message(STATUS "ZLIB: ${ZLIB_VERSION}")

find_path(TINYXML2_INCLUDE_DIR "tinyxml2.h")
find_library(TINYXML2_LIBRARIES NAMES "tinyxml2")

find_path(GL2PS_INCLUDE_DIR "gl2ps.h")
find_library(GL2PS_LIBRARIES NAMES "gl2ps")

find_path(MINIZIP_INCLUDE_DIR minizip/unzip.h PATH_SUFFIXES minizip)
find_library(MINIZIP_LIBRARIES NAMES "minizip")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# --- Vendored LDView engine libraries -----------------------------------
#
# ldview/TRE, ldview/TCFoundation, ldview/LDExporter, ldview/LDLoader and
# ldview/LDLib have no .pro/CMake target of their own -- each ships only a
# tiny GNU Makefile (see ldview/Makefile.inc). ldview/QT/CMakeLists.txt
# already builds this exact dependency graph against Qt6 by shelling out to
# `make` via ExternalProject_Add; this section reuses that recipe (verified
# to build cleanly against Qt 6.11 on macOS/clang) rather than re-deriving
# it, with three corrections found while doing so:
#
#   1. ldview/QT/CMakeLists.txt never sets USE_CPP17=YES on the `make`
#      invocation, so each library builds with -std=c++11 (Makefile.inc's
#      own default). Qt6 headers require C++17
#      ("Qt requires a C++17 compiler"), so any file that includes a Qt
#      header (TCFoundation, TRE, LDLoader, LDLib) fails to build as-is.
#   2. On a framework-based Qt install (the normal case on macOS -- Qt
#      headers live inside e.g. QtCore.framework/Headers, not under a
#      plain -I path), `#include <QtCore/qstring.h>`-style includes used
#      throughout TCFoundation/mystring.h etc. need `-F<dir containing the
#      .framework bundles>` on the compile line, which
#      ldview/QT/CMakeLists.txt never passes to the sub-`make` builds
#      (it only sets Qt include dirs for the top-level LDView target).
#   3. TCFoundation's TESTING var in ldview/QT/CMakeLists.txt hardcodes
#      `-I/usr/include/qt6`, a Linux-distro-specific path that does not
#      exist on macOS (or on other Linux layouts). Replaced below with
#      Qt's own reported include dirs.
#
# Only the pieces LSculpt's preview needs are built: model loading +
# OpenGL rendering + basic view controls, via TRE/LDLib/LDLoader/
# TCFoundation/LDExporter + lib3ds. LDView-app-only surface (snapshot/
# export dialogs, library auto-updater, part/model-tree browsers,
# printing, LDViewThumbs, Launcher, BuildPartsCatalog, installers) is
# intentionally not built.

set(LDVIEW_EXTRA_QT_FLAGS "")
if(APPLE)
	get_filename_component(QT_FRAMEWORK_DIR "${Qt${QT_VERSION_MAJOR}_DIR}/../.." ABSOLUTE)
	set(LDVIEW_EXTRA_QT_FLAGS "-F${QT_FRAMEWORK_DIR}")
else()
	foreach(_dir ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS})
		string(APPEND LDVIEW_EXTRA_QT_FLAGS " -I${_dir}")
	endforeach()
endif()

set(LDVIEW_LIB_POSTFIX "-qt${QT_VERSION_MAJOR}")
set(LDVIEW_COMMON_DEFS "-D_NO_BOOST -DHAVE_MINIZIP -DUSE_CPP11 ${LDVIEW_EXTRA_QT_FLAGS}")

include(ExternalProject)

ExternalProject_Add(TRE
	SOURCE_DIR ${LDVIEW_DIR}/TRE
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} POSTFIX=${LDVIEW_LIB_POSTFIX} USE_CPP17=YES TESTING=${LDVIEW_COMMON_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/TRE/libTRE${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(TCFoundation
	SOURCE_DIR ${LDVIEW_DIR}/TCFoundation
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} POSTFIX=${LDVIEW_LIB_POSTFIX} USE_CPP17=YES TESTING=-D_NO_BOOST\ -D_QT\ -DHAVE_MINIZIP\ -DUSE_CPP11\ ${LDVIEW_EXTRA_QT_FLAGS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/TCFoundation/libTCFoundation${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(LDExporter
	SOURCE_DIR ${LDVIEW_DIR}/LDExporter
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} POSTFIX=${LDVIEW_LIB_POSTFIX} USE_CPP17=YES TESTING=${LDVIEW_COMMON_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/LDExporter/libLDExporter${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(LDLoader
	SOURCE_DIR ${LDVIEW_DIR}/LDLoader
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} POSTFIX=${LDVIEW_LIB_POSTFIX} USE_CPP17=YES TESTING=-D_NO_BOOST\ -D_QT\ -DHAVE_MINIZIP\ -DUSE_CPP11\ ${LDVIEW_EXTRA_QT_FLAGS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/LDLoader/libLDLoader${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(LDLib
	SOURCE_DIR ${LDVIEW_DIR}/LDLib
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} POSTFIX=${LDVIEW_LIB_POSTFIX} USE_CPP17=YES TESTING=${LDVIEW_COMMON_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/LDLib/libLDraw${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(lib3ds
	SOURCE_DIR ${LDVIEW_DIR}/3rdParty/lib3ds
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} USE_CPP17=YES
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/3rdParty/lib3ds/lib3ds.a
)

# --- LSculpt GUI target --------------------------------------------------
# Full GUI wiring (lsculptmainwin/argpanel/aboutdialog + the new
# Qt-native LDraw viewer widget that replaces LDVLib) lands in a later
# step; this target intentionally does not exist yet.
