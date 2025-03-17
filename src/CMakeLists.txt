add_library(src STATIC observer.cpp
        observer.h
        draw.cpp
        draw.h
        common.h)
target_include_directories(src PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
