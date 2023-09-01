#
# Copyright 2023 Suzuki Yoshinori(wave.suzuki.z@gmail.com)
#
cmake_minimum_required(VERSION 3.20)
enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_CXX_FLAGS "-g -O0")

add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT=1 -DDEFAULT_PORT=44528)

project(fsrv)

find_package(OpenSSL REQUIRED)

include_directories(include)
include_directories(external/cpp-httplib)
include_directories(external/json/include)
include_directories(external/cxxopts/include)
include_directories(${OPENSSL_INCLUDE_DIR})

set(srv_src
    src/server/main.cpp
)

set(cli_src
    src/client/main.cpp
)

add_executable(fsrv ${srv_src})
target_link_libraries(fsrv PRIVATE ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

add_executable(fcli ${cli_src})
target_link_libraries(fcli PRIVATE ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
