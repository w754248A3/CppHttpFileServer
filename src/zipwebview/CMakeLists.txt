add_executable(zipwebview src/zipwebview.cpp)

target_compile_definitions(zipwebview PUBLIC
        _WIN32_WINNT=0x0602
        NTDDI_VERSION=0x06020000
)

target_include_directories(zipwebview PUBLIC
        ${MY_DIR_PATH}/../bit7z-master/include/bit7z
    )



# 关键：启用 Unicode 入口点支持 (让链接器找 wmain 而不是 main)
target_link_options(zipwebview PRIVATE -static -municode)

target_link_directories(zipwebview PUBLIC
    ${MY_DIR_PATH}/../bit7z-master/lib/x64

)

target_link_libraries(zipwebview PRIVATE
    mypublicapi
    myserverapi
    boost_json-mt
    bit7z64
    oleaut32
    uuid
    ws2_32
    wininet
    shlwapi)


my_set_target_out(zipwebview ZipWebView)