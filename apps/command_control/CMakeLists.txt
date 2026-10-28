cmake_minimum_required(VERSION 3.10)

find_package(SDL2 REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

add_executable(command_control main.cpp)
set_target_properties(command_control PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

target_include_directories(command_control PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/idl/generated
    ${CMAKE_SOURCE_DIR}/idl
)

target_link_libraries(command_control PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
)

add_custom_command(TARGET command_control POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/ArleighBurke-class.png"
        "$<TARGET_FILE_DIR:command_control>/ArleighBurke-class.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Stylized Orange Phalanx CIWS Firing.png"
        "$<TARGET_FILE_DIR:command_control>/Stylized Orange Phalanx CIWS Firing.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/USER_QOS_PROFILES.xml"
        "$<TARGET_FILE_DIR:command_control>/USER_QOS_PROFILES.xml"
)

if(WIN32)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(command_control PRIVATE SDL2::SDL2main)
    endif()
    set(_VCPKG_BIN "C:/RTI/vcpkg/installed/x64-windows/bin")
    add_custom_command(TARGET command_control POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_BIN}/SDL2_image.dll"
            "$<TARGET_FILE_DIR:command_control>/SDL2_image.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_BIN}/libpng16.dll"
            "$<TARGET_FILE_DIR:command_control>/libpng16.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VCPKG_BIN}/z.dll"
            "$<TARGET_FILE_DIR:command_control>/z.dll"
    )
endif()

if(USE_CONNEXT)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
        target_link_libraries(command_control PRIVATE PkgConfig::GTK3)
    endif()
    find_path(RTI_CONNEXTDDS_INCLUDE ndds/ndds_cpp.h PATHS ${RTI_CONNEXTDDS_DIR}/include)

    find_library(RTI_LIB_NDDSCPP  nddscpp  PATHS ${RTI_LIB_DIR} NO_DEFAULT_PATH)
    find_library(RTI_LIB_NDDSC    nddsc    PATHS ${RTI_LIB_DIR} NO_DEFAULT_PATH)
    find_library(RTI_LIB_NDDSCORE nddscore PATHS ${RTI_LIB_DIR} NO_DEFAULT_PATH)
    set(RTI_CONNEXTDDS_LIBS ${RTI_LIB_NDDSCPP} ${RTI_LIB_NDDSC} ${RTI_LIB_NDDSCORE})

    if(NOT RTI_CONNEXTDDS_INCLUDE OR NOT RTI_LIB_NDDSCPP OR NOT RTI_LIB_NDDSC OR NOT RTI_LIB_NDDSCORE)
        message(FATAL_ERROR "RTI Connext headers or dynamic libraries not found. Set RTI_CONNEXTDDS_DIR to the RTI install root.")
    endif()

    target_compile_definitions(command_control PRIVATE USE_CONNEXT ${RTI_PLATFORM_DEFINITIONS})
    target_sources(command_control PRIVATE threat_qos.cpp)
    target_include_directories(command_control PRIVATE
        ${RTI_CONNEXTDDS_INCLUDE}
        ${RTI_CONNEXTDDS_DIR}/include/ndds
    )
    if(TARGET ship_types)
        target_link_libraries(command_control PRIVATE ship_types ${RTI_CONNEXTDDS_LIBS})
    else()
        target_link_libraries(command_control PRIVATE ${RTI_CONNEXTDDS_LIBS})
    endif()
    if(NOT WIN32)
        set_property(TARGET command_control APPEND PROPERTY BUILD_RPATH "${RTI_LIB_DIR}")
    endif()
endif()
