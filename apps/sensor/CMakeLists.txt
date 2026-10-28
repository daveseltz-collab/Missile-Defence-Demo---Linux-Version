cmake_minimum_required(VERSION 3.10)

add_executable(sensor main.cpp)
set_target_properties(sensor PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

add_custom_command(TARGET sensor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/USER_QOS_PROFILES.xml"
        "$<TARGET_FILE_DIR:sensor>/USER_QOS_PROFILES.xml"
)

target_include_directories(sensor PRIVATE
    ${CMAKE_SOURCE_DIR}/idl/generated
    ${CMAKE_SOURCE_DIR}/idl
)

if(USE_CONNEXT)
    find_path(RTI_CONNEXTDDS_INCLUDE ndds/ndds_cpp.h PATHS ${RTI_CONNEXTDDS_DIR}/include)

    find_library(RTI_LIB_NDDSCPP  nddscpp  PATHS ${RTI_LIB_DIR} NO_DEFAULT_PATH)
    find_library(RTI_LIB_NDDSC    nddsc    PATHS ${RTI_LIB_DIR} NO_DEFAULT_PATH)
    find_library(RTI_LIB_NDDSCORE nddscore PATHS ${RTI_LIB_DIR} NO_DEFAULT_PATH)
    set(RTI_CONNEXTDDS_LIBS ${RTI_LIB_NDDSCPP} ${RTI_LIB_NDDSC} ${RTI_LIB_NDDSCORE})

    if(NOT RTI_CONNEXTDDS_INCLUDE OR NOT RTI_LIB_NDDSCPP OR NOT RTI_LIB_NDDSC OR NOT RTI_LIB_NDDSCORE)
        message(FATAL_ERROR "RTI Connext headers or dynamic libraries not found. Set RTI_CONNEXTDDS_DIR to the RTI install root.")
    endif()
    target_compile_definitions(sensor PRIVATE USE_CONNEXT ${RTI_PLATFORM_DEFINITIONS})
    target_include_directories(sensor PRIVATE
        ${RTI_CONNEXTDDS_INCLUDE}
        ${RTI_CONNEXTDDS_DIR}/include/ndds
    )
    if(TARGET ship_types)
        target_link_libraries(sensor PRIVATE ship_types ${RTI_CONNEXTDDS_LIBS})
    else()
        target_link_libraries(sensor PRIVATE ${RTI_CONNEXTDDS_LIBS})
    endif()
    if(NOT WIN32)
        set_property(TARGET sensor APPEND PROPERTY BUILD_RPATH "${RTI_LIB_DIR}")
    endif()
endif()
