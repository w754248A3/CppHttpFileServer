# 定义静态库
add_library(myserverapi STATIC src/myserverapi.cpp)


target_include_directories(myserverapi
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)


target_link_libraries(myserverapi PRIVATE
    mypublicapi
    boost_json-mt
    oleaut32
    uuid
    ws2_32
    wininet
    shlwapi)