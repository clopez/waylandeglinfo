cmake_minimum_required(VERSION 3.12)
project(waylandeglinfo VERSION 0.2)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(CheckIncludeFile)

set(WAYLANDEGLINFO_SOURCES waylandes2info.c)

# Tri-state toggles: AUTO, ON, OFF
set(WAYLANDEGLINFO_GLES AUTO CACHE STRING "Build GLES2 variant: AUTO, ON, OFF")
set_property(CACHE WAYLANDEGLINFO_GLES PROPERTY STRINGS AUTO ON OFF)
set(WAYLANDEGLINFO_GL   AUTO CACHE STRING "Build desktop OpenGL variant: AUTO, ON, OFF")
set_property(CACHE WAYLANDEGLINFO_GL PROPERTY STRINGS AUTO ON OFF)


find_package(EGL REQUIRED)
find_package(Wayland REQUIRED)
check_include_file("GLES2/gl2.h" HAVE_GLES2_H)
check_include_file("GL/gl.h"     HAVE_GL_H)

# Probe GLES
set(BUILD_GLES FALSE)
if(NOT WAYLANDEGLINFO_GLES STREQUAL "OFF")
  find_package(OpenGLES2 QUIET)
  if(OpenGLES2_FOUND AND HAVE_GLES2_H)
    set(BUILD_GLES TRUE)
  endif()
endif()

# Probe desktop GL
set(BUILD_GL FALSE)
if(NOT WAYLANDEGLINFO_GL STREQUAL "OFF")
  find_package(OpenGL QUIET)
  if((TARGET OpenGL::GL OR OPENGL_gl_LIBRARY) AND HAVE_GL_H)
    set(BUILD_GL TRUE)
  endif()
endif()

# Honor explicit ON
if(WAYLANDEGLINFO_GLES STREQUAL "ON" AND NOT BUILD_GLES)
  message(FATAL_ERROR "GLES requested but not found (headers or libraries missing).")
endif()
if(WAYLANDEGLINFO_GL STREQUAL "ON" AND NOT BUILD_GL)
  message(FATAL_ERROR "Desktop OpenGL requested but not found (headers or libraries missing).")
endif()

if(NOT BUILD_GLES AND NOT BUILD_GL)
  message(FATAL_ERROR "Neither OpenGLES2 nor desktop OpenGL are available. Install headers and libs, or adjust CMake options.")
endif()

# Common include/lib groups (Wayland + EGL)
set(WAYINFO_COMMON_INCLUDES
    ${EGL_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
)
set(WAYINFO_COMMON_LIBS
    ${EGL_LIBRARIES}
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
)

# ---- GLES2 binary (waylandeglinfo-es) ----
if(BUILD_GLES)
  add_executable(waylandeglinfo-es ${WAYLANDEGLINFO_SOURCES})
  target_compile_definitions(waylandeglinfo-es PRIVATE
    WAYLANDEGLINFO_USE_GLES=1
    ${OPENGLES2_DEFINITIONS} ${EGL_DEFINITIONS} ${WAYLAND_DEFINITIONS} ${WAYLAND_EGL_DEFINITIONS}
  )
  target_include_directories(waylandeglinfo-es PRIVATE
    ${WAYINFO_COMMON_INCLUDES}
    ${OPENGLES2_INCLUDE_DIRS}
  )
  target_link_libraries(waylandeglinfo-es PRIVATE
    ${WAYINFO_COMMON_LIBS}
    ${OPENGLES2_LIBRARIES}
  )
  install(TARGETS waylandeglinfo-es DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
  message(STATUS "Enabled: waylandeglinfo-es (GLES2)")
endif()

# ---- Desktop GL binary (waylandeglinfo-gl) ----
if(BUILD_GL)
  add_executable(waylandeglinfo-gl ${WAYLANDEGLINFO_SOURCES})
  target_compile_definitions(waylandeglinfo-gl PRIVATE
    WAYLANDEGLINFO_USE_GL=1
    ${EGL_DEFINITIONS} ${WAYLAND_DEFINITIONS} ${WAYLAND_EGL_DEFINITIONS}
  )
  if(TARGET OpenGL::GL)
    target_link_libraries(waylandeglinfo-gl PRIVATE ${WAYINFO_COMMON_LIBS} OpenGL::GL)
  else()
    target_link_libraries(waylandeglinfo-gl PRIVATE ${WAYINFO_COMMON_LIBS} ${OPENGL_gl_LIBRARY})
  endif()
  if(DEFINED OPENGL_INCLUDE_DIR)
    target_include_directories(waylandeglinfo-gl PRIVATE ${WAYINFO_COMMON_INCLUDES} ${OPENGL_INCLUDE_DIR})
  else()
    target_include_directories(waylandeglinfo-gl PRIVATE ${WAYINFO_COMMON_INCLUDES})
  endif()
  install(TARGETS waylandeglinfo-gl DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
  message(STATUS "Enabled: waylandeglinfo-gl (desktop OpenGL)")
endif()
