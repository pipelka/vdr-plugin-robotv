cmake_minimum_required(VERSION 2.8)
project(robotvdmx)

# set C++11 for robotv
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    include/robotvdmx/aaccommon.h
    include/robotvdmx/ac3common.h
    include/robotvdmx/demuxer.h
    include/robotvdmx/demuxerbundle.h
    include/robotvdmx/pes.h
    include/robotvdmx/streambundle.h
    include/robotvdmx/streaminfo.h
    src/demuxer.cpp
    src/demuxerbundle.cpp
    src/streambundle.cpp
    src/streaminfo.cpp
    src/parsers/parser_ac3.cpp
    src/parsers/parser_ac3.h
    src/parsers/parser_adts.cpp
    src/parsers/parser_adts.h
    src/parsers/parser_h264.cpp
    src/parsers/parser_h264.h
    src/parsers/parser_h265.cpp
    src/parsers/parser_h265.h
    src/parsers/parser_latm.cpp
    src/parsers/parser_latm.h
    src/parsers/parser_mpegaudio.cpp
    src/parsers/parser_mpegaudio.h
    src/parsers/parser_mpegvideo.cpp
    src/parsers/parser_mpegvideo.h
    src/parsers/parser_pes.cpp
    src/parsers/parser_pes.h
    src/parsers/parser_subtitle.cpp
    src/parsers/parser_subtitle.h
    src/parsers/parser.cpp
    src/parsers/parser.h
    src/upstream/ringbuffer.cpp
    src/upstream/ringbuffer.h
    src/upstream/bitstream.h
    src/upstream/bitstream.cpp)

add_library(robotvdmx STATIC ${SOURCE_FILES})
target_compile_options(robotvdmx PRIVATE -fPIC)
target_include_directories(robotvdmx PUBLIC include PRIVATE src)

#set_target_properties(robotvdmx PROPERTIES VERSION "${VDR_APIVERSION}")
#install(TARGETS robotvdmx LIBRARY DESTINATION ${VDR_LIBDIR} NAMELINK_SKIP)
