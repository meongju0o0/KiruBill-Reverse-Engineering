cmake_minimum_required(VERSION 3.16)
project(KiruBill CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp)
set(ENGINE_SOURCES
  src/engine/app.cpp
  src/engine/gfx.cpp
  src/engine/input.cpp
  src/engine/music.cpp
  src/engine/sound.cpp
  src/engine/task.cpp
  src/engine/testing.cpp)

if(WIN32)
  add_executable(KiruBill WIN32 ${ENGINE_SOURCES} ${GAME_SOURCES} src/platform/win32.cpp src/platform/kirubill.rc)
  target_link_libraries(KiruBill PRIVATE winmm dsound opengl32 gdi32 user32 shell32)
  target_compile_definitions(KiruBill PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    # bit exact float semantics, static CRT -> no redistributable needed
    target_compile_options(KiruBill PRIVATE /fp:precise /utf-8 /W3)
    set_property(TARGET KiruBill PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  else()
    target_compile_options(KiruBill PRIVATE -fno-strict-aliasing -fwrapv -ffp-contract=off -Wall)
    target_link_options(KiruBill PRIVATE -static -static-libgcc -static-libstdc++ $<$<CONFIG:Release>:-s>)
  endif()
else()
  find_package(SDL2 REQUIRED)
  find_package(Freetype)
  find_package(Threads REQUIRED)
  add_executable(KiruBill ${ENGINE_SOURCES} ${GAME_SOURCES} src/platform/sdl.cpp)
  target_include_directories(KiruBill PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(KiruBill PRIVATE ${SDL2_LIBRARIES} Threads::Threads)
  if(FREETYPE_FOUND)
    target_compile_definitions(KiruBill PRIVATE HAVE_FREETYPE)
    target_link_libraries(KiruBill PRIVATE Freetype::Freetype)
  endif()
  # -fwrapv: the original relies on 32-bit two's complement wrap-around
  target_compile_options(KiruBill PRIVATE -fno-strict-aliasing -fwrapv -ffp-contract=off -Wall)
endif()

set_target_properties(KiruBill PROPERTIES OUTPUT_NAME "KiruBill")
