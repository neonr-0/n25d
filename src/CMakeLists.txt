add_library(n25d STATIC n25d.c n25dfile.c)
target_include_directories(n25d PUBLIC
    ${CMAKE_SOURCE_DIR}/headers
    ${CMAKE_SOURCE_DIR}/depends)
