# 定义静态库
add_library(mypublicapi STATIC src/mypublicapi.cpp)


target_include_directories(mypublicapi 
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
