cmake_minimum_required(VERSION 3.15)

set(LUNASVG_VERSION_MAJOR 3)
set(LUNASVG_VERSION_MINOR 5)
set(LUNASVG_VERSION_MICRO 0)

project(lunasvg LANGUAGES C CXX VERSION ${LUNASVG_VERSION_MAJOR}.${LUNASVG_VERSION_MINOR}.${LUNASVG_VERSION_MICRO})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/dist)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/dist)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/dist)
endforeach()

include(CheckIncludeFile)
check_include_file("threads.h" HAVE_THREADS_H)

set(lunasvg_sources
    source/lunasvg/lunasvg.cpp
    source/lunasvg/graphics.cpp
    source/lunasvg/element.cpp
    source/lunasvg/filterelement.cpp
    source/lunasvg/geometryelement.cpp
    source/lunasvg/layoutstate.cpp
    source/lunasvg/paintelement.cpp
    source/lunasvg/parser.cpp
    source/lunasvg/property.cpp
    source/lunasvg/renderstate.cpp
    source/lunasvg/textelement.cpp
    # plutovg sources
    source/plutovg/blend.c
    source/plutovg/canvas.c
    source/plutovg/font.c
    source/plutovg/ft-math.c
    source/plutovg/ft-raster.c
    source/plutovg/ft-stroker.c
    source/plutovg/matrix.c
    source/plutovg/paint.c
    source/plutovg/path.c
    source/plutovg/rasterize.c
    source/plutovg/surface.c
)

set(lunasvg_headers
    include/lunasvg.h
    include/plutovg.h
    source/lunasvg/graphics.h
    source/lunasvg/element.h
    source/lunasvg/filterelement.h
    source/lunasvg/geometryelement.h
    source/lunasvg/layoutstate.h
    source/lunasvg/paintelement.h
    source/lunasvg/parserutils.h
    source/lunasvg/property.h
    source/lunasvg/renderstate.h
    source/lunasvg/textelement.h
    # plutovg headers
    source/plutovg/ft-math.h
    source/plutovg/ft-raster.h
    source/plutovg/ft-stroker.h
    source/plutovg/ft-types.h
    source/plutovg/private.h
    source/plutovg/stb-image.h
    source/plutovg/stb-image-write.h
    source/plutovg/stb-truetype.h
    source/plutovg/utils.h
)

add_library(lunasvg ${lunasvg_sources} ${lunasvg_headers})
add_library(lunasvg::lunasvg ALIAS lunasvg)

set_target_properties(lunasvg PROPERTIES
    SOVERSION ${LUNASVG_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    CXX_STANDARD_REQUIRED ON
    CXX_STANDARD 17
    C_STANDARD_REQUIRED ON
    C_STANDARD 11
)

target_include_directories(lunasvg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source/lunasvg
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plutovg
)

target_include_directories(lunasvg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/lunasvg>
)

target_compile_definitions(lunasvg PRIVATE LUNASVG_BUILD PLUTOVG_BUILD)
if(HAVE_THREADS_H)
    target_compile_definitions(lunasvg PRIVATE HAVE_THREADS_H)
endif()

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(lunasvg PUBLIC LUNASVG_BUILD_STATIC PLUTOVG_BUILD_STATIC)
endif()

option(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS "Disable automatic loading of fonts from system directories" OFF)
if(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS)
    target_compile_definitions(lunasvg PRIVATE LUNASVG_DISABLE_LOAD_SYSTEM_FONTS)
    target_compile_definitions(lunasvg PRIVATE PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD)
endif()

find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(lunasvg PRIVATE m)
endif()

find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(lunasvg PRIVATE Threads::Threads)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lunasvgConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lunasvgConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lunasvg
)

write_basic_package_version_file(lunasvgConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lunasvg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/plutovg.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lunasvg
)

install(TARGETS lunasvg
    EXPORT lunasvgTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT lunasvgTargets
    FILE lunasvgTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lunasvg
    NAMESPACE lunasvg::
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lunasvgConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/lunasvgConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lunasvg
)

export(EXPORT lunasvgTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/lunasvgTargets.cmake
    NAMESPACE lunasvg::
)

file(RELATIVE_PATH lunasvg_pc_prefix_relative
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    "${CMAKE_INSTALL_PREFIX}"
)

set(lunasvg_pc_cflags "")
set(lunasvg_pc_libs_private "")

if(MATH_LIBRARY)
    string(APPEND lunasvg_pc_libs_private " -lm")
endif()

if(Threads_FOUND AND CMAKE_THREAD_LIBS_INIT)
    string(APPEND lunasvg_pc_libs_private " ${CMAKE_THREAD_LIBS_INIT}")
endif()

if(NOT BUILD_SHARED_LIBS)
    string(APPEND lunasvg_pc_cflags " -DLUNASVG_BUILD_STATIC -DPLUTOVG_BUILD_STATIC")
endif()

string(CONFIGURE [[
prefix=${pcfiledir}/@lunasvg_pc_prefix_relative@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@
libdir=${prefix}/@CMAKE_INSTALL_LIBDIR@

Name: LunaSVG
Description: SVG rendering and manipulation library
Version: @PROJECT_VERSION@

Cflags: -I${includedir}/lunasvg@lunasvg_pc_cflags@
Libs: -L${libdir} -llunasvg
Libs.private:@lunasvg_pc_libs_private@
]] lunasvg_pc @ONLY)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lunasvg.pc" "${lunasvg_pc}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lunasvg.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

option(LUNASVG_BUILD_EXAMPLES "Build examples" ON)
if(LUNASVG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

add_executable(convert tools/convert.cpp)
target_link_libraries(convert PRIVATE lunasvg)
set_target_properties(convert PROPERTIES CXX_STANDARD 17)