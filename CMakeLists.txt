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
#
# Only defined -- and only claims the "LSculpt" output name -- when
# LSCULPT_CONSOLE is actually requested. Previously this target was added
# unconditionally with OUTPUT_NAME "LSculpt", the same name the full GUI
# executable below also claims; both landed in the same output directory,
# so an ordinary default build (LSCULPT_CONSOLE=OFF) silently let whichever
# target linked last overwrite the other's binary on disk -- confirmed by
# building and finding the console CLI's argv-parsing main() sitting where
# the Qt GUI app was expected.
find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

if(LSCULPT_CONSOLE)
	add_executable(LSculptConsole
		LSculpt.cpp
		rply.c
		obj_parser.cpp
	)
	target_compile_definitions(LSculptConsole PRIVATE LSCULPT_CONSOLE)
	target_link_libraries(LSculptConsole Qt${QT_VERSION_MAJOR}::Core)
	set_target_properties(LSculptConsole PROPERTIES OUTPUT_NAME "LSculpt")

	return()
endif()

# --- Everything below here is the full GUI build ------------------------

set(OpenGL_GL_PREFERENCE LEGACY)

find_package(QT NAMES Qt6 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets OpenGL OpenGLWidgets REQUIRED)
find_package(OpenGL REQUIRED)
# Not requested via find_package(OpenGL COMPONENTS GLU): on at least one
# tested Linux/GLVND setup that COMPONENTS check spuriously fails ("Could
# NOT find OpenGL (missing: GLU)") even though find_library below locates
# it fine immediately after -- an apparent FindOpenGL.cmake/GLVND
# interaction, not a real absence of GLU. Find it directly instead.
if(APPLE)
	# GLU's symbols live inside OpenGL.framework itself on macOS -- there's
	# no separate libGLU to find (mirrors FindOpenGL.cmake's own Apple case).
	set(LDVIEW_GLU_LIBRARY ${OPENGL_LIBRARIES})
else()
	find_library(LDVIEW_GLU_LIBRARY NAMES GLU MesaGLU glu32 REQUIRED)
endif()
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

# tinyxml2/gl2ps aren't always available as system/toolchain packages --
# confirmed on a MinGW-w64 cross build (Fedora's mingw64 repo has neither
# mingw64-tinyxml2 nor mingw64-gl2ps, only mingw64-minizip). Fall back to
# compiling ldview's own vendored single-file copies directly into LSculpt
# (same pattern as ldview/QT/misc.cpp above) instead of requiring the user
# to source/package these themselves.
set(LDVIEW_VENDOR_SOURCES "")
if(NOT TINYXML2_INCLUDE_DIR OR NOT TINYXML2_LIBRARIES)
	message(STATUS "tinyxml2: system package not found, vendoring ldview/3rdParty/tinyxml/tinyxml2.cpp")
	set(TINYXML2_INCLUDE_DIR ${LDVIEW_DIR}/3rdParty/tinyxml)
	set(TINYXML2_LIBRARIES "")
	list(APPEND LDVIEW_VENDOR_SOURCES ${LDVIEW_DIR}/3rdParty/tinyxml/tinyxml2.cpp)
endif()
if(NOT GL2PS_INCLUDE_DIR OR NOT GL2PS_LIBRARIES)
	message(STATUS "gl2ps: system package not found, vendoring ldview/3rdParty/gl2ps/gl2ps.c")
	set(GL2PS_INCLUDE_DIR ${LDVIEW_DIR}/3rdParty/gl2ps)
	set(GL2PS_LIBRARIES "")
	list(APPEND LDVIEW_VENDOR_SOURCES ${LDVIEW_DIR}/3rdParty/gl2ps/gl2ps.c)
endif()

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

# TCFoundation/TCDefines.h's TCExport macro defaults to
# __declspec(dllimport) on WIN32 unless _TC_STATIC (or
# _BUILDING_TCFOUNDATION_LIB) is defined -- it assumes TCFoundation is a
# DLL by default. We build it as a static lib, so every Windows compile
# unit that touches a TCObject-derived type (both inside the engine libs
# and in LSculpt's own ldrawpreviewwidget.cpp) needs _TC_STATIC, or the
# linker looks for nonexistent "__imp_..." DLL import-thunk symbols --
# confirmed by actually cross-building for Windows (MinGW) and hitting
# exactly that. Only TCFoundation has this dllexport/dllimport pattern;
# TRE/LDLoader/LDLib/LDExporter don't need an equivalent define.
#
# Separately: the whole ldview codebase checks the bare `#ifdef WIN32`
# (not `_WIN32`) for Windows-specific code (TREGL.h's winsock2.h/GL
# headers vs. sys/socket.h+GL/glx-style headers, TCNetwork.h's socket
# API, etc). MinGW-w64 GCC predefines WIN32 by default -- but only in GNU
# mode; `-std=c++17` (used above for Qt6, a strict ISO mode) suppresses
# it, since GCC only predefines non-reserved (no leading underscore)
# macros like bare WIN32 outside strict-conformance modes, while the
# reserved `_WIN32` survives. Confirmed by actually cross-building for
# Windows (MinGW) and hitting TREGL.h/TCNetwork.h silently taking the
# non-Windows branch (missing <sys/socket.h> etc) despite targeting
# Windows. Defining WIN32 explicitly (matching how real MSVC Windows
# projects always do it via project settings, never relying on implicit
# compiler predefinition) fixes this regardless of -std= mode.
#
# Separately, Makefile.inc's own `-std=c++17` (added via USE_CPP17=YES,
# always LAST on the compile line, after $(TESTING)) would conflict with
# any -std= we put in TESTING, since GCC honors the LAST -std= flag on the
# command line -- confirmed by trying to set -std=gnu++17 via TESTING
# while still passing USE_CPP17=YES, and getting strict c++17 (and its
# WIN32-suppression bug above) regardless. Fix: don't request
# USE_CPP17=YES for Windows at all, and put `-std=gnu++17` in TESTING
# instead so it's the only -std= flag on the line.
set(LDVIEW_WIN32_DEFS "")
set(LDVIEW_WIN32_USE_CPP17 "USE_CPP17=YES")
set(LDVIEW_WIN32_TCFOUNDATION_CFLAGSLOC "")
if(WIN32)
	set(LDVIEW_WIN32_DEFS "-D_TC_STATIC -DWIN32 -std=gnu++17")
	set(LDVIEW_WIN32_USE_CPP17 "")
	# TCFoundation/Makefile hardcodes CFLAGSLOC = -O3 -DUNZIP_CMD
	# unconditionally, on every platform. UNZIP_CMD makes
	# TCFoundation/TCUnzip.cpp shell out to an external `unzip` binary via
	# fork()/wait()/execv() -- POSIX-only calls with no Windows
	# implementation or guard in that function, so it simply fails to
	# compile ("'fork' was not declared") on MinGW, confirmed by actually
	# cross-building for Windows. TCUnzip already has a perfectly good
	# Windows-appropriate path (HAVE_MINIZIP, in-process, already defined
	# above) for exactly this reason; override CFLAGSLOC to drop
	# -DUNZIP_CMD on Windows and let it use that instead.
	set(LDVIEW_WIN32_TCFOUNDATION_CFLAGSLOC "CFLAGSLOC=-O3")
endif()

set(LDVIEW_LIB_POSTFIX "-qt${QT_VERSION_MAJOR}")
set(LDVIEW_COMMON_DEFS "-D_NO_BOOST -DHAVE_MINIZIP -DUSE_CPP11 ${LDVIEW_EXTRA_QT_FLAGS} ${LDVIEW_WIN32_DEFS}")

# Makefile.inc picks its own CC/CXX by `uname` (clang on Darwin, else plain
# "gcc"/"g++") with no awareness of cross-compilation. That's a silent
# correctness bug, not just a MinGW-cross concern: verified by actually
# cross-building for Windows from Linux via a MinGW-w64 toolchain file
# (CMAKE_C_COMPILER=x86_64-w64-mingw32-gcc) -- without this, Makefile.inc's
# internal `CC = gcc` assignment picks the Linux host's native gcc
# regardless, silently producing a native ELF build with none of the
# Windows headers CMake's own find_package(...) calls resolved. Passing
# CC/CXX explicitly makes every ExternalProject_Add use whatever compiler
# CMake itself resolved (native or cross), the same way the app target
# below already does automatically.
#
# AR is deliberately NOT overridden this way: Makefile.inc's Darwin branch
# uses `libtool -static -o` rather than plain `ar rcs`, and that's load-
# bearing, not a leftover -- verified by trying `${CMAKE_AR} rcs` (plain
# /usr/bin/ar) on macOS, which silently produces an archive Apple's own
# linker then fails to read ("archive member '/' not a mach-o file").
# Elsewhere, plain `ar rcs` is correct, so only force it cross-platform
# outside Apple.
# CMAKE_OSX_ARCHITECTURES (single-arch cross builds like arm64->x86_64
# Rosetta targeting, or "arm64;x86_64" universal builds) is honored
# automatically for the LSculpt/Qt targets CMake itself compiles and links,
# but these engine libs are built by shelling out to plain `make`, which
# knows nothing about it and would otherwise let clang pick its native/host
# arch -- producing static archives whose arch doesn't match the app being
# linked ("building for X, but linking in object file built for Y"). Fold
# the requested -arch flags directly into CC/CXX so every sub-make compile
# (and, since AR isn't overridden on Apple -- see below -- the libtool
# archive step, which reads them straight off the object files) matches.
set(LDVIEW_ARCH_FLAGS "")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
	foreach(_arch ${CMAKE_OSX_ARCHITECTURES})
		string(APPEND LDVIEW_ARCH_FLAGS " -arch ${_arch}")
	endforeach()
endif()

if(APPLE)
	set(LDVIEW_MAKE_TOOLCHAIN "CC=${CMAKE_C_COMPILER}${LDVIEW_ARCH_FLAGS}" "CXX=${CMAKE_CXX_COMPILER}${LDVIEW_ARCH_FLAGS}")
else()
	set(LDVIEW_MAKE_TOOLCHAIN CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} "AR=${CMAKE_AR} rcs")
endif()

include(ExternalProject)

ExternalProject_Add(TRE
	SOURCE_DIR ${LDVIEW_DIR}/TRE
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${LDVIEW_MAKE_TOOLCHAIN} POSTFIX=${LDVIEW_LIB_POSTFIX} ${LDVIEW_WIN32_USE_CPP17} TESTING=${LDVIEW_COMMON_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/TRE/libTRE${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(TCFoundation
	SOURCE_DIR ${LDVIEW_DIR}/TCFoundation
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${LDVIEW_MAKE_TOOLCHAIN} POSTFIX=${LDVIEW_LIB_POSTFIX} ${LDVIEW_WIN32_USE_CPP17} ${LDVIEW_WIN32_TCFOUNDATION_CFLAGSLOC} TESTING=-D_NO_BOOST\ -D_QT\ -DHAVE_MINIZIP\ -DUSE_CPP11\ ${LDVIEW_EXTRA_QT_FLAGS}\ ${LDVIEW_WIN32_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/TCFoundation/libTCFoundation${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(LDExporter
	SOURCE_DIR ${LDVIEW_DIR}/LDExporter
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${LDVIEW_MAKE_TOOLCHAIN} POSTFIX=${LDVIEW_LIB_POSTFIX} ${LDVIEW_WIN32_USE_CPP17} TESTING=${LDVIEW_COMMON_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/LDExporter/libLDExporter${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(LDLoader
	SOURCE_DIR ${LDVIEW_DIR}/LDLoader
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${LDVIEW_MAKE_TOOLCHAIN} POSTFIX=${LDVIEW_LIB_POSTFIX} ${LDVIEW_WIN32_USE_CPP17} TESTING=-D_NO_BOOST\ -D_QT\ -DHAVE_MINIZIP\ -DUSE_CPP11\ ${LDVIEW_EXTRA_QT_FLAGS}\ ${LDVIEW_WIN32_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/LDLoader/libLDLoader${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(LDLib
	SOURCE_DIR ${LDVIEW_DIR}/LDLib
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${LDVIEW_MAKE_TOOLCHAIN} POSTFIX=${LDVIEW_LIB_POSTFIX} ${LDVIEW_WIN32_USE_CPP17} TESTING=${LDVIEW_COMMON_DEFS}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/LDLib/libLDraw${LDVIEW_LIB_POSTFIX}.a
)

ExternalProject_Add(lib3ds
	SOURCE_DIR ${LDVIEW_DIR}/3rdParty/lib3ds
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${LDVIEW_MAKE_TOOLCHAIN} ${LDVIEW_WIN32_USE_CPP17}
	INSTALL_COMMAND ""
	BUILD_BYPRODUCTS ${LDVIEW_DIR}/3rdParty/lib3ds/lib3ds.a
)

# --- LSculpt GUI target --------------------------------------------------
# Replaces the old Windows-only LDVLib.dll preview with ldrawpreviewwidget
# (a QOpenGLWidget embedding LDrawModelViewer directly -- see
# ldrawpreviewwidget.h for the LDVLib-call -> direct-call mapping).
#
# ldview/QT/misc.cpp supplies ucstringtoqstring()/wcstoqstring(), which
# TCFoundation/TCUserDefaults.cpp calls but does not define or link itself
# -- ldview/QT/CMakeLists.txt only gets away with omitting it from the
# library build because it globs misc.cpp into the *app* target alongside
# TCFoundation. Since LSculpt doesn't build the LDView app, pull in just
# this one small, self-contained (no LDView-app dependencies) file.
add_executable(LSculpt
	LSculpt.cpp
	rply.c
	obj_parser.cpp
	main.cpp
	lsculptmainwin.cpp
	argpanel.cpp
	aboutdialog.cpp
	ldrawpreviewwidget.cpp
	${LDVIEW_DIR}/QT/misc.cpp
	${LDVIEW_VENDOR_SOURCES}
	lsculptmainwin.ui
	argpanel.ui
	aboutdialog.ui
	advanced_widgets.ui
	LSculpt.qrc
)

target_include_directories(LSculpt PRIVATE ${LDVIEW_DIR} ${LDVIEW_DIR}/QT ${TINYXML2_INCLUDE_DIR} ${GL2PS_INCLUDE_DIR})
if(WIN32)
	# See the LDVIEW_WIN32_DEFS comment above: LSculpt's own code (e.g.
	# ldrawpreviewwidget.cpp) touches TCObject-derived types directly too,
	# and transitively includes ldview headers that check bare WIN32.
	target_compile_definitions(LSculpt PRIVATE _TC_STATIC WIN32)
endif()

target_link_libraries(LSculpt
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::OpenGL
	Qt${QT_VERSION_MAJOR}::OpenGLWidgets
	${CMAKE_THREAD_LIBS_INIT}
)

# The five engine libs mutually reference each other's symbols (e.g. LDLib
# needs TCFoundation's TCObject, TCFoundation callbacks reference back into
# LDLib types). GNU ld resolves static-archive symbols in a single
# left-to-right pass and will NOT revisit an earlier archive to satisfy a
# later one's undefined reference, so a fixed link order fails there
# (verified: this exact link order fails with "undefined reference to
# TCObject::..." from LDLib on both Linux/GNU-ld and Windows/MinGW's GNU
# ld, and succeeds unwrapped on macOS, whose linker resolves archives more
# liberally). --start-group/--end-group makes ld re-scan the group until
# everything resolves; it's a GNU ld extension Apple's linker doesn't have
# (and doesn't need), so apply it everywhere except Apple.
set(LDVIEW_ENGINE_LIBS
	-L${LDVIEW_DIR}/LDLib -lLDraw${LDVIEW_LIB_POSTFIX}
	-L${LDVIEW_DIR}/TRE -lTRE${LDVIEW_LIB_POSTFIX}
	-L${LDVIEW_DIR}/LDLoader -lLDLoader${LDVIEW_LIB_POSTFIX}
	-L${LDVIEW_DIR}/LDExporter -lLDExporter${LDVIEW_LIB_POSTFIX}
	-L${LDVIEW_DIR}/TCFoundation -lTCFoundation${LDVIEW_LIB_POSTFIX}
)
if(NOT APPLE)
	target_link_libraries(LSculpt -Wl,--start-group ${LDVIEW_ENGINE_LIBS} -Wl,--end-group)
else()
	target_link_libraries(LSculpt ${LDVIEW_ENGINE_LIBS})
endif()

# PNG/JPEG/ZLIB must come after the engine-lib group above: TCFoundation
# calls into libpng/libjpeg/zlib (TCPngImageFormat/TCJpegImageFormat/the
# minizip path) but doesn't link them itself, and GNU ld (Linux) errors
# "DSO missing from command line" if a shared lib a static archive needs
# isn't given explicitly, positioned after that archive -- macOS's linker
# resolves this transitively and doesn't need it spelled out.
# GLU (gluOrtho2D/gluPerspective/gluBuild2DMipmaps, used by LDLib/TRE) is a
# separate library from GL proper and, like PNG/JPEG/ZLIB above, must come
# after the engine-lib group on Linux. OPENGL_LIBRARIES itself moved down
# here too: TRE calls glGetError() directly and, on Windows/MinGW, an
# opengl32 appearing before the engine-lib group left it as an unresolved
# "__imp_glGetError" -- confirmed by actually cross-building for Windows.
target_link_libraries(LSculpt
	${GL2PS_LIBRARIES} ${TINYXML2_LIBRARIES} ${MINIZIP_LIBRARIES}
	${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES}
	${LDVIEW_GLU_LIBRARY} ${OPENGL_LIBRARIES}
	-L${LDVIEW_DIR}/3rdParty/lib3ds -l3ds
)

if(UNIX AND NOT APPLE)
	target_link_libraries(LSculpt ${X11_X11_LIB})
endif()
if(WIN32)
	# TCWebClient/TCNetworkClient call WSAGetLastError/WSASetLastError
	# (Winsock) directly but never link ws2_32 themselves -- same
	# DSO-after-consumer ordering requirement as PNG/JPEG/ZLIB/GLU above.
	target_link_libraries(LSculpt ws2_32)
endif()

add_dependencies(LSculpt TRE TCFoundation LDExporter LDLoader LDLib lib3ds)

# LSculptMainWin's constructor (lsculptmainwin.cpp) looks for an "LDVLib/"
# folder next to the built executable (QApplication::applicationDirPath()) --
# empty.ldr plus the PARTS/P LDraw parts library the preview and voxelizer
# both need -- and calls exit(1) with a "cannot find the LDraw parts"
# message box if it's missing. The data lives in the repo at release/LDVLib
# (checked in for the old Windows/qmake build, which built straight into
# that directory), but nothing put a copy next to the CMake build's own
# output executable. Copy it there after each build so a fresh CMake build
# is runnable without manually reproducing the qmake layout.
add_custom_command(TARGET LSculpt POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/release/LDVLib
		$<TARGET_FILE_DIR:LSculpt>/LDVLib
)
