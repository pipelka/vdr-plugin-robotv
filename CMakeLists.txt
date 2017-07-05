cmake_minimum_required(VERSION 2.8)
project(robotv)

set(PLUGIN "robotv")
set(ROBOTV_VERSION "0.10.3")

# check for pkg-config
find_package(PkgConfig REQUIRED)

# check for vdr
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_CURRENT_SOURCE_DIR}/../../..")
pkg_check_modules(VDR vdr=2.2.0)

macro(vdr_find_variable _variable)
    execute_process(
            COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=${_variable} vdr
            OUTPUT_VARIABLE _variable_result)
    string(REGEX REPLACE "[\r\n]" "" _variable_result "${_variable_result}")
    string(TOUPPER "vdr_${_variable}" _variable_upper)
    _pkgconfig_set(${_variable_upper} ${_variable_result})
endmacro(vdr_find_variable)

# get vdr installation variables
vdr_find_variable(apiversion)
vdr_find_variable(locdir)
vdr_find_variable(libdir)
vdr_find_variable(configdir)
vdr_find_variable(cachedir)

message(STATUS "API Version: '${VDR_APIVERSION}'")
message(STATUS "Locales: '${VDR_LOCDIR}'")
message(STATUS "Libraries: '${VDR_LIBDIR}'")
message(STATUS "Configuration: '${VDR_CONFIGDIR}'")
message(STATUS "Cache: '${VDR_CACHEDIR}'")

# set C++11 for robotv
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# sqlite
set(SQLITE_FILES
    src/db/sqlite3.c
    src/db/sqlite3.h
    src/db/sqlite3ext.h)

add_library(sqlite STATIC ${SQLITE_FILES})
target_compile_definitions(sqlite PRIVATE HAVE_USLEEP SQLITE_THREADSAFE=1 SQLITE_ENABLE_FTS4)
target_compile_options(sqlite PRIVATE -fPIC)
target_link_libraries(sqlite dl pthread)

# robotv plugin
set(SOURCE_FILES
    src/config/config.cpp
    src/config/config.h
    src/db/database.cpp
    src/db/database.h
    src/db/storage.cpp
    src/db/storage.h
    src/epg/epghandler.cpp
    src/epg/epghandler.h
    src/live/channelcache.cpp
    src/live/channelcache.h
    src/live/livequeue.cpp
    src/live/livequeue.h
    src/live/livestreamer.cpp
    src/live/livestreamer.h
    src/net/msgpacket.cpp
    src/net/msgpacket.h
    src/net/os-config.cpp
    src/net/os-config.h
    src/recordings/artwork.cpp
    src/recordings/artwork.h
    src/recordings/packetplayer.cpp
    src/recordings/packetplayer.h
    src/recordings/recordingscache.cpp
    src/recordings/recordingscache.h
    src/recordings/recplayer.cpp
    src/recordings/recplayer.h
    src/robotv/controllers/artworkcontroller.cpp
    src/robotv/controllers/artworkcontroller.h
    src/robotv/controllers/channelcontroller.cpp
    src/robotv/controllers/channelcontroller.h
    src/robotv/controllers/controller.h
    src/robotv/controllers/epgcontroller.cpp
    src/robotv/controllers/epgcontroller.h
    src/robotv/controllers/logincontroller.cpp
    src/robotv/controllers/logincontroller.h
    src/robotv/controllers/moviecontroller.cpp
    src/robotv/controllers/moviecontroller.h
    src/robotv/controllers/recordingcontroller.cpp
    src/robotv/controllers/recordingcontroller.h
    src/robotv/controllers/streamcontroller.cpp
    src/robotv/controllers/streamcontroller.h
    src/robotv/controllers/timercontroller.cpp
    src/robotv/controllers/timercontroller.h
    src/robotv/svdrp/channelcmds.cpp
    src/robotv/svdrp/channelcmds.h
    src/robotv/robotv.cpp
    src/robotv/robotv.h
    src/robotv/robotvclient.cpp
    src/robotv/robotvclient.h
    src/robotv/robotvcommand.h
    src/robotv/robotvserver.cpp
    src/robotv/robotvserver.h
    src/scanner/wirbelscan.cpp
    src/scanner/wirbelscan.h
    src/scanner/wirbelscan_services.h
    src/tools/utf8/checked.h
    src/tools/utf8/core.h
    src/tools/utf8/unchecked.h
    src/tools/hash.cpp
    src/tools/hash.h
    src/tools/json.hpp
    src/tools/recid2uid.cpp
    src/tools/recid2uid.h
    src/tools/time.cpp
    src/tools/time.h
    src/tools/urlencode.cpp
    src/tools/urlencode.h
    src/tools/utf8.h
    src/tools/utf8conv.h
    src/tools/utf8conv.cpp)

add_subdirectory(src/demuxer)

add_library(vdr-robotv SHARED ${SOURCE_FILES})
target_link_libraries(vdr-robotv robotvdmx sqlite z)
target_include_directories(vdr-robotv PRIVATE src ${VDR_INCLUDE_DIRS} ../../../include)
target_compile_definitions(vdr-robotv PRIVATE ROBOTV_VERSION="${ROBOTV_VERSION}" PLUGIN_NAME_I18N="${PLUGIN}" HAVE_ZLIB=1)
set_target_properties(vdr-robotv PROPERTIES VERSION "${VDR_APIVERSION}")

install(TARGETS vdr-robotv LIBRARY DESTINATION ${VDR_LIBDIR} NAMELINK_SKIP)
