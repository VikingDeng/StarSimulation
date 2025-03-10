add_library(dataset STATIC dataset.cpp)
target_include_directories(dataset PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
