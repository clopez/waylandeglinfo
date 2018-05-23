cmake_minimum_required(VERSION 3.1)
project(waylandeglinfo VERSION 0.1)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GNUInstallDirs)


find_package(EGL REQUIRED)
find_package(Wayland REQUIRED)
find_package(WaylandEGL REQUIRED)
find_package(OpenGLES2 REQUIRED)

set(WAYLANDEGLINFO_SOURCES waylandes2info.c)

set(WAYLANDEGLINFO_INCLUDE_DIRECTORIES
    ${OPENGLES2_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
)

set(WAYLANDEGLINFO_LIBRARIES
    ${OPENGLES2_LIBRARIES}
    ${EGL_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
)

add_definitions(
    ${OPENGLES2_DEFINITIONS}
    ${EGL_DEFINITIONS}
    ${WAYLAND_DEFINITIONS}
    ${WAYLAND_EGL_DEFINITIONS}
)


add_executable(waylandeglinfo ${WAYLANDEGLINFO_SOURCES})
target_include_directories(waylandeglinfo PUBLIC ${WAYLANDEGLINFO_INCLUDE_DIRECTORIES})
target_link_libraries (waylandeglinfo ${WAYLANDEGLINFO_LIBRARIES})

install(TARGETS waylandeglinfo DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT "runtime")
