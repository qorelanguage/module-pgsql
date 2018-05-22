
cmake_minimum_required(VERSION 3.0.0)

project(qore-pgsql-module VERSION 2.4.1)

option(enable-scu "build as a single compilation unit" ON)
option(server-includes "build with pgsql server includes" OFF)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CheckCXXSymbolExists)
include(CheckIncludeFileCXX)

# Check for C++11.
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

find_package(Qore 0.9 REQUIRED)
qore_find_pthreads()

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

qore_wrap_qpp_value(QPP_SOURCES ${QPP_SRC})
add_custom_target(QPP_GENERATED_FILES DEPENDS ${QPP_SOURCES})

set(module_name "pgsql")

if(enable-scu)
    add_library(${module_name} SHARED src/single-compilation-unit.cpp)
    add_dependencies(${module_name} QPP_GENERATED_FILES)
else(enable-scu)
    add_library(${module_name} SHARED ${CPP_SRC} ${QPP_SOURCES})
endif(enable-scu)

if (WIN32 AND MINGW AND MSYS)
    target_compile_definitions(${module_name} PUBLIC BUILDING_DLL)
endif (WIN32 AND MINGW AND MSYS)

if(server-includes)
    target_compile_definitions(${module_name} PUBLIC POSTGRESQL_SERVER_INCLUDES)
endif(server-includes)

qore_binary_module(${module_name} ${PROJECT_VERSION} ${PostgreSQL_LIBRARY} Threads::Threads)

qore_dist(${PROJECT_VERSION})

qore_config_info()
