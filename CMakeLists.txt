cmake_minimum_required(VERSION 3.10)
project(MissileDefense LANGUAGES CXX)

option(USE_CONNEXT "Build the communicating RTI Connext DDS demo" ON)

# Prefer an explicit variable, but fall back to the NDDSHOME environment variable
if(NOT DEFINED RTI_CONNEXTDDS_DIR)
  if(DEFINED ENV{NDDSHOME})
    set(RTI_CONNEXTDDS_DIR $ENV{NDDSHOME})
    message(STATUS "Using NDDSHOME for RTI Connext: ${RTI_CONNEXTDDS_DIR}")
  endif()
endif()

if(WIN32)
  set(RTI_PLATFORM_LIB_DIR x64Win64VS2017)
  set(RTI_PLATFORM_DEFINITIONS
    RTI_WIN32
    RTI_dds_c_DLL_VARIABLE
    RTI_dds_cpp_DLL_VARIABLE
    RTI_osapi_DLL_VARIABLE
    RTI_cdr_DLL_VARIABLE
    RTI_log_DLL_VARIABLE
  )
else()
  set(RTI_PLATFORM_DEFINITIONS RTI_UNIX RTI_LINUX)
  if(DEFINED ENV{CONNEXTDDS_ARCH})
    set(RTI_PLATFORM_LIB_DIR "$ENV{CONNEXTDDS_ARCH}")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(RTI_PLATFORM_LIB_DIR x64Linux4gcc8.5.0)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(RTI_PLATFORM_LIB_DIR armv8Linux4gcc7.3.0)
  else()
    message(FATAL_ERROR "Unsupported RTI Connext architecture: ${CMAKE_SYSTEM_PROCESSOR}. Set CONNEXTDDS_ARCH.")
  endif()
endif()

set(RTI_LIB_DIR "${RTI_CONNEXTDDS_DIR}/lib/${RTI_PLATFORM_LIB_DIR}")

# If RTI-generated IDL sources exist, build them into a static library usable by apps
if(USE_CONNEXT)
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/dds-library-path.txt"
       CONTENT "${RTI_LIB_DIR}\n")
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/idl/generated/ShipThreat.h")
    message(FATAL_ERROR "DDS type support is missing. Set NDDSHOME to the selected RTI installation and run scripts/generate_rtiddsgen.sh first.")
  endif()
  file(GLOB RTI_GENERATED_SOURCES ${CMAKE_SOURCE_DIR}/idl/generated/*.cxx ${CMAKE_SOURCE_DIR}/idl/generated/*.cpp)
  if(NOT RTI_GENERATED_SOURCES)
    message(FATAL_ERROR "DDS type-support sources are missing. Run scripts/generate_rtiddsgen.sh.")
  endif()
  if(RTI_GENERATED_SOURCES)
    add_library(ship_types STATIC ${RTI_GENERATED_SOURCES})
    target_include_directories(ship_types PRIVATE ${CMAKE_SOURCE_DIR}/idl ${CMAKE_SOURCE_DIR}/idl/generated)
    if(DEFINED RTI_CONNEXTDDS_DIR)
      target_include_directories(ship_types PRIVATE ${RTI_CONNEXTDDS_DIR}/include ${RTI_CONNEXTDDS_DIR}/include/ndds)
    endif()
    target_compile_definitions(ship_types PRIVATE ${RTI_PLATFORM_DEFINITIONS})
    set_target_properties(ship_types PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
  endif()
endif()

# Add project subdirectories (apps, tests, examples)
if(EXISTS ${CMAKE_SOURCE_DIR}/apps/CMakeLists.txt)
  add_subdirectory(apps)
endif()
