# CMake build system for CMatrix-Pipes

cmake_minimum_required(VERSION 2.8)

project(CMatrix-Pipes LANGUAGES C)
set(VERSION "2.0")

include(GNUInstallDirs)

# These are relative to CMAKE_INSTALL_PREFIX
# which by default is "/usr/local"
set(CONSOLE_FONTS_DIRS "share/consolefonts" "lib/kbd/consolefonts")
set(X_FONTS_DIRS "lib/X11/fonts/misc" "X11R6/lib/X11/fonts/misc" "share/fonts/X11/misc")

set(MKFONTDIR "/usr/bin/mkfontdir")

message(STATUS "Looking for include file config.h")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/config.h")
    add_definitions(-DEXCLUDE_CONFIG_H)
    message(STATUS "Looking for include file config.h - missing")
else()
    message(STATUS "Looking for include file config.h - found")
endif()

# configure_file(config.h.in config.h)

add_definitions(-DVERSION="${VERSION}")
if	(NOT WIN32)
	add_definitions(-DUSE_TIOCSTI)
endif	()

include(CheckIncludeFiles)
check_include_files("sys/ioctl.h" HAVE_SYS_IOCTL_H)
if	(HAVE_SYS_IOCTL_H)
	add_definitions(-DHAVE_SYS_IOCTL_H)
endif	()
check_include_files("unistd.h" HAVE_UNISTD_H)
if	(HAVE_UNISTD_H)
	add_definitions(-DHAVE_UNISTD_H)
endif	()
check_include_files("termios.h" HAVE_TERMIOS_H)
if	(HAVE_TERMIOS_H)
	add_definitions(-DHAVE_TERMIOS_H)
endif	()
check_include_files("termio.h" HAVE_TERMIO_H)
if	(HAVE_TERMIO_H)
	add_definitions(-DHAVE_TERMIO_H)
endif	()
check_include_files("getopt.h" HAVE_GETOPT_H)
if	(HAVE_GETOPT_H)
	add_definitions(-DHAVE_GETOPT_H)
endif	()

Set(CURSES_NEED_NCURSES TRUE)
Set(CURSES_NEED_WIDE TRUE)
find_package(Curses)
include_directories(${CURSES_INCLUDE_DIR})
add_definitions(-DHAVE_NCURSES_H)

include(CheckSymbolExists)
list(APPEND CMAKE_REQUIRED_LIBRARIES ncurses)
check_symbol_exists(use_default_colors "ncurses.h" HAVE_USE_DEFAULT_COLORS)
if (HAVE_USE_DEFAULT_COLORS)
    add_definitions(-DHAVE_USE_DEFAULT_COLORS)
endif()

add_executable(cmatrix-pipes cmatrix.c)

find_package(Threads REQUIRED)
target_link_libraries(cmatrix-pipes ${CURSES_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS cmatrix-pipes DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES cmatrix.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

if     (UNIX)
	foreach    (CONSOLE_FONTS_DIR ${CONSOLE_FONTS_DIRS})
		if     (IS_DIRECTORY "${CMAKE_INSTALL_PREFIX}/${CONSOLE_FONTS_DIR}")
			message(STATUS "Installing matrix console fonts to ${CMAKE_INSTALL_PREFIX}/${CONSOLE_FONTS_DIR}")
			install(FILES
				"${CMAKE_SOURCE_DIR}/matrix.fnt"
				"${CMAKE_SOURCE_DIR}/matrix.psf.gz"
				DESTINATION "${CONSOLE_FONTS_DIR}")
		endif  ()
	endforeach ()
	foreach    (X_FONTS_DIR ${X_FONTS_DIRS})
		if     (IS_DIRECTORY "${CMAKE_INSTALL_PREFIX}/${X_FONTS_DIR}")
			message(STATUS "Installing matrix X window fonts to ${CMAKE_INSTALL_PREFIX}/${X_FONTS_DIR}")
			install(FILES
				"${CMAKE_SOURCE_DIR}/mtx.pcf"
				DESTINATION "${X_FONTS_DIR}")
			install(CODE
				"message(STATUS \"Running mkfontdir ${CMAKE_INSTALL_PREFIX}/${X_FONTS_DIR} ...\")")
			install(CODE
				"execute_process(COMMAND \"${MKFONTDIR}\" \"${CMAKE_INSTALL_PREFIX}/${X_FONTS_DIR}\")")
			install(CODE
				"message(STATUS \"If this is the first time you have installed CMatrix-Pipes you will probably have to restart X window in order to use the mtx.pcf font.\")")
		endif  ()
	endforeach ()
endif  ()