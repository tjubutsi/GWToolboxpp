cmake_minimum_required(VERSION 3.16)

set(CMAKE_GENERATOR_PLATFORM win32)

project(gwtoolbox)

set(GWTOOLBOXEXE_VERSION "4.0")
set(GWTOOLBOXDLL_VERSION "6.0")
set(GWTOOLBOXDLL_VERSION_BETA "") # can be anything. Marks beta version if not empty.
set(GWTOOBOX_MODULE_VERSION "6,0,0,0") # used for Dll module info. See GWToolboxdll/GWToolbox.rc

# Updates toolboxversion.txt after changes in the above version (make sure to run cmake .. or build the project)
file(WRITE "resources/toolboxversion.txt" ${GWTOOLBOXDLL_VERSION})

# this is no longer required, but in case we have to add an installer again some day
# configure_file(${PROJECT_SOURCE_DIR}/gwtoolbox.wxs.in ${PROJECT_SOURCE_DIR}/gwtoolbox.wxs @ONLY)

if(MSVC_TOOLSET_VERSION LESS 142)
    message(FATAL_ERROR "Build using Visual Studio 2019 toolset (v142) or higher.")
endif()

if(NOT(CMAKE_SIZEOF_VOID_P EQUAL 4))
    message(FATAL_ERROR "You are configuring a non 32-bit build, this is not supported. Run cmake with `-A Win32`")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_definitions(
    "NOMINMAX"
    "_USE_MATH_DEFINES"
    "WIN32_LEAN_AND_MEAN"

    "GWTOOLBOXEXE_VERSION=\"${GWTOOLBOXEXE_VERSION}\""
    "GWTOOLBOXDLL_VERSION=\"${GWTOOLBOXDLL_VERSION}\""
    "GWTOOLBOXDLL_VERSION_BETA=\"${GWTOOLBOXDLL_VERSION_BETA}\""
    "GWTOOLBOX_MODULE_VERSION=${GWTOOBOX_MODULE_VERSION}"
    )

add_compile_options(/MP /permissive-)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Outputs dll, exe, and pdb into a /bin/config folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/")

include(gwca)
include(imgui)
include(directxtex)
include(utf8proc)
include(easywsclient)
include(simpleini)
include(discord_game_sdk)
include(earcut)

add_subdirectory(GWToolboxdll)
add_subdirectory(Core)
add_subdirectory(RestClient)
add_subdirectory(GWToolbox)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GWToolbox)

# Below is for toolbox plugins (WIP):
add_library(module_base INTERFACE)
target_sources(module_base INTERFACE
    "modules/base/dllmain.cpp"
    "modules/base/module_base.h")
target_include_directories(module_base INTERFACE "modules/base")
target_include_directories(module_base INTERFACE "GWToolboxdll/GWToolbox")
target_link_libraries(module_base INTERFACE
    imgui
    earcut
    simpleini
    gwca
    )
target_compile_definitions(module_base INTERFACE BUILD_DLL)

macro(add_tb_module MODULE)
    add_library(${MODULE} SHARED)
    file(GLOB SOURCES
        "${PROJECT_SOURCE_DIR}/modules/${MODULE}/*.h"
        "${PROJECT_SOURCE_DIR}/modules/${MODULE}/*.cpp")
    target_sources(${MODULE} PRIVATE ${SOURCES})
    target_include_directories(${MODULE} PRIVATE "${PROJECT_SOURCE_DIR}/modules/${MODULE}")
    target_link_libraries(${MODULE} PRIVATE module_base)
    set_target_properties(${MODULE} PROPERTIES FOLDER "Modules/")
endmacro()

# add_tb_module(InstanceTimer)
# add_tb_module(clock)
