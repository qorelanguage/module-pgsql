
cmake_minimum_required(VERSION 3.0.0)

project(qore-pgsql-module VERSION 2.4)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CheckCXXSymbolExists)
include(CheckIncludeFileCXX)

find_package(Qore 0.8.12 REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
message(STATUS "Found POSIX Threads: TRUE")
else(CMAKE_USE_PTHREADS_INIT)
message(STATUS "Found POSIX Threads: FALSE")
message(FATAL_ERROR "POSIX threads does not seem to be supported on this platform, aborting")
endif()

find_package(PostgreSQL 7.0.0 REQUIRED)

# check if the client library has PQLibVersion
set(CMAKE_REQUIRED_INCLUDES ${PostgreSQL_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${PostgreSQL_LIBRARY})
check_cxx_symbol_exists(PQlibVersion libpq-fe.h HAVE_PQLIBVERSION)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_LIBRARIES)

check_include_file_cxx(arpa/inet.h HAVE_ARPA_INET_H)
check_include_file_cxx(netinet/in.h HAVE_NETINET_IN_H)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${PostgreSQL_INCLUDE_DIRS})
add_definitions(-DQORE_MODULE_PACKAGE_VERSION="${PROJECT_VERSION}")

check_cxx_compiler_flag(-fvisibility=hidden HAVE_GCC_VISIBILITY)

configure_file(${CMAKE_SOURCE_DIR}/cmake/config.h.cmake
               ${CMAKE_BINARY_DIR}/config.h)

set(QPP_SRC src/ql_pgsql.qpp)

set(CPP_SRC
    src/pgsql.cpp
    src/QorePGConnection.cpp
    src/QorePGMapper.cpp
)

qore_wrap_qpp(QPP_SOURCES ${QPP_SRC})
add_custom_target(QPP_GENERATED_FILES DEPENDS ${QPP_SOURCES})

set(module_name "pgsql")

add_library(${module_name} SHARED src/single-compilation-unit.cpp)
add_dependencies(${module_name} QPP_GENERATED_FILES)

qore_binary_module(${module_name} ${PROJECT_VERSION} ${PostgreSQL_LIBRARY} Threads::Threads)

qore_dist(${PROJECT_VERSION})

qore_config_info()
