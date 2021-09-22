project(hvl)

cmake_minimum_required(VERSION 3.5)

set(SOURCES blip_buf.c
            blip_buf.h
            hvl_replay.c
            hvl_replay.h
            hvl_tables.h
            sinc_generator.c)

add_library(hvl STATIC ${SOURCES})
set_target_properties(hvl PROPERTIES POSITION_INDEPENDENT_CODE 1)
