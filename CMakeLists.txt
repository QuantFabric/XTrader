cmake_minimum_required(VERSION 3.10)

PROJECT(XTrader)
# 输出so至build目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/)

set(CMAKE_CXX_FLAGS "-fPIC")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET(AppVersion 0.9.3)
SET(XTrader_Version ${PROJECT_NAME}_${AppVersion})

# 默认输出debug版
#SET(CMAKE_BUILD_TYPE "Release")
#SET(CMAKE_BUILD_TYPE "debug")
SET(CMAKE_BUILD_TYPE "RelWithDebInfo")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g -fPIC -O3 -Wfatal-errors -Wl,-export-dynamic")

add_definitions(-DFMT_HEADER_ONLY)
add_definitions(-DFMTLOG_HEADER_ONLY)
add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

macro(Get_CommitID APP_COMMITID)
    find_package(Git QUIET)     # 查找Git，QUIET静默方式不报错
    if(GIT_FOUND)
      execute_process(          # 执行一个子进程
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD # 命令
        OUTPUT_VARIABLE ${APP_COMMITID}        # 输出字符串存入变量
        OUTPUT_STRIP_TRAILING_WHITESPACE    # 删除字符串尾的换行符
        ERROR_QUIET                         # 对执行错误静默
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} # 执行路径
        )
    endif()
endmacro()

macro(Get_UtilsCommitID GIT_UTILSCOMMITID)
    find_package(Git QUIET)     # 查找Git，QUIET静默方式不报错
    if(GIT_FOUND)
      execute_process(          # 执行一个子进程
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD # 命令
        OUTPUT_VARIABLE ${GIT_UTILSCOMMITID}        # 输出字符串存入变量
        OUTPUT_STRIP_TRAILING_WHITESPACE    # 删除字符串尾的换行符
        ERROR_QUIET                         # 对执行错误静默
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../Utils/ # 执行路径
        )
    endif()
endmacro()

macro(Get_SHMServerCommitID GIT_SHMSERVERCOMMITID)
    find_package(Git QUIET)     # 查找Git，QUIET静默方式不报错
    if(GIT_FOUND)
      execute_process(          # 执行一个子进程
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD # 命令
        OUTPUT_VARIABLE ${GIT_SHMSERVERCOMMITID}        # 输出字符串存入变量
        OUTPUT_STRIP_TRAILING_WHITESPACE    # 删除字符串尾的换行符
        ERROR_QUIET                         # 对执行错误静默
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../SHMServer/ # 执行路径
        )
    endif()
endmacro()

macro(Get_GitCommited_Date GIT_COMMITED_DATE)
    find_package(Git QUIET)     # 查找Git，QUIET静默方式不报错
    if(GIT_FOUND)
      execute_process(          # 执行一个子进程
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=short # 命令
        OUTPUT_VARIABLE ${GIT_COMMITED_DATE}        # 输出字符串存入变量
        OUTPUT_STRIP_TRAILING_WHITESPACE    # 删除字符串尾的换行符
        ERROR_QUIET                         # 对执行错误静默
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} # 执行路径
        )
    endif()
endmacro()

macro(Get_GitVersion GIT_VERSION)
    find_package(Git QUIET)     # 查找Git，QUIET静默方式不报错
    if(GIT_FOUND)
      execute_process(          # 执行一个子进程
        COMMAND ${GIT_EXECUTABLE} describe --abbrev=0 # 命令
        OUTPUT_VARIABLE ${GIT_VERSION}        # 输出字符串存入变量
        OUTPUT_STRIP_TRAILING_WHITESPACE    # 删除字符串尾的换行符
        ERROR_QUIET                         # 对执行错误静默
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} # 执行路径
        )
    endif()
endmacro()

set(APP_COMMITID "")
Get_CommitID(APP_COMMITID)
message(STATUS "AppCommitID is ${APP_COMMITID}")

set(UTILS_COMMITID "")
Get_UtilsCommitID(UTILS_COMMITID)
message(STATUS "UtilsCommitID is ${UTILS_COMMITID}")

set(SHMSERVER_COMMITID "")
Get_SHMServerCommitID(SHMSERVER_COMMITID)
message(STATUS "SHMServerCommitID is ${SHMSERVER_COMMITID}")

set(GIT_COMMITED_DATE "")
Get_GitCommited_Date(GIT_COMMITED_DATE)
message(STATUS "GitCommitedDate is ${GIT_COMMITED_DATE}")

set(GIT_VERSION "")
Get_GitVersion(GIT_VERSION)
message(STATUS "GitVersion is ${GIT_VERSION}")

add_definitions(-DAPP_COMMITID=\"${APP_COMMITID}\")
add_definitions(-DUTILS_COMMITID=\"${UTILS_COMMITID}\")
add_definitions(-DSHMSERVER_COMMITID=\"${SHMSERVER_COMMITID}\")
add_definitions(-DGIT_COMMITED_DATE=\"${GIT_COMMITED_DATE}\")
add_definitions(-DGIT_VERSION=\"${GIT_VERSION}\")

file(WRITE ${PROJECT_BINARY_DIR}/GitVersion.h
    "#define APP_COMMITID \"${APP_COMMITID}\"\n#define UTILS_COMMITID \"${UTILS_COMMITID}\"\n#define GIT_VERSION \"${GIT_VERSION}\"\n#define GIT_COMMITED_DATE \"${GIT_COMMITED_DATE}\"\n"
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Utils/)

# HP-Scoket
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XAPI/HP-Socket/5.8.2/include/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XAPI/HP-Socket/5.8.2/lib)

# FMTLogger
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XAPI/FMTLogger/include/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XAPI/FMTLogger/lib)

# YAML-CPP
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XAPI/YAML-CPP/0.8.0/include/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XAPI/YAML-CPP/0.8.0/lib)

# SHMServer
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../SHMServer/)

# Parallel HashMap
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../XAPI/parallel_hashmap/)

set(SOURCES main.cpp 
            HPPackClient.cpp
            TraderEngine.cpp)
add_executable(${XTrader_Version} ${SOURCES})

target_link_libraries(${XTrader_Version} pthread hpsocket4c yaml-cpp dl rt)      

# 编译API SO
add_subdirectory(TraderAPI)
