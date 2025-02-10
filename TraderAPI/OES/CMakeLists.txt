cmake_minimum_required(VERSION 3.16)
PROJECT(OESTrader)

set(CMAKE_CXX_FLAGS "-fPIC")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET(AppVersion 0.4.0)
SET(APIVersion OES0.17.4.1)
SET(TradeGateWay_Version ${PROJECT_NAME}_${AppVersion})

# 默认输出debug版
#SET(CMAKE_BUILD_TYPE "Release")
#SET(CMAKE_BUILD_TYPE "debug")
SET(CMAKE_BUILD_TYPE "RelWithDebInfo")
set(CMAKE_CXX_STANDARD 17)

add_definitions(-DFMT_HEADER_ONLY)
add_definitions(-DFMTLOG_HEADER_ONLY)
#add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

macro(Get_CommitID SO_COMMITID)
    find_package(Git QUIET)     # 查找Git，QUIET静默方式不报错
    if(GIT_FOUND)
      execute_process(          # 执行一个子进程
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD # 命令
        OUTPUT_VARIABLE ${SO_COMMITID}        # 输出字符串存入变量
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
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../Utils/ # 执行路径
        )
    endif()
endmacro()

set(SO_COMMITID "")
Get_CommitID(SO_COMMITID)
message(STATUS "SoCommitID is ${SO_COMMITID}")

set(SO_UTILS_COMMITID "")
Get_UtilsCommitID(SO_UTILS_COMMITID)
message(STATUS "SoUtilsCommitID is ${SO_UTILS_COMMITID}")

add_definitions(-DSO_COMMITID=\"${SO_COMMITID}\")
add_definitions(-DSO_UTILS_COMMITID=\"${SO_UTILS_COMMITID}\")
add_definitions(-DAPI_VERSION=\"${APIVersion}\")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../Utils/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../TraderAPI/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../)

# FMTLogger
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../XAPI/FMTLogger/include/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../XAPI/FMTLogger/lib)
# YAML-CPP
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../XAPI/YAML-CPP/0.6.3/include/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../XAPI/YAML-CPP/0.6.3/lib)
# OES
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../XAPI/OES/0.17.4.1/include/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../XAPI/OES/0.17.4.1/lib/)

# Parallel HashMap
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../XAPI/parallel_hashmap/)

set(SOURCES OESTradeGateWay.cpp OESTraderAPI.cpp)
add_library(${TradeGateWay_Version} SHARED ${SOURCES})

target_link_libraries(${TradeGateWay_Version} pthread yaml-cpp oes_0.17.4.1)   

            
