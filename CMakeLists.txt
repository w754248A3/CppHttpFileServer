cmake_minimum_required(VERSION 3.16)


if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()


project(CppHttpFileServer CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(MY_DIR_PATH ${PROJECT_SOURCE_DIR})

# --- 新增：强制使用静态库 ---
set(Boost_USE_STATIC_LIBS ON) 
# 有时候还需要开启静态运行时（取决于 MSYS2 的具体包配置，建议加上）
set(Boost_USE_STATIC_RUNTIME OFF)


add_compile_options(
    -pedantic-errors
    -Wsign-conversion
    -Woverflow
    -Wconversion
    -Werror
    -Wextra
    -fconcepts

)


function(my_set_target_out targetname filename)
    # 设置输出目录
    set_target_properties(${targetname} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${MY_DIR_PATH}/out
    OUTPUT_NAME "${filename}"
    )
endfunction()


# 3. 查找依赖
# MSYS2 安装的 Boost 通常可以自动找到
#find_package(Boost REQUIRED COMPONENTS json headers CONFIG)
# find_package(OtherLib REQUIRED) 

# 4. 添加子目录
add_subdirectory(src/mypublicapi) # 先构建公共/PCH部分
add_subdirectory(src/myserverapi)
add_subdirectory(src/zipwebview)
