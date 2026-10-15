cmake_minimum_required(VERSION 3.21)
project(ApeEscapeNativeDemo LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(SDL3 3.2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Same source list as tools/codex/build_native_viewer.py (all optional features compiled in).
# Includes between modules are relative, so no include directories are needed.
set(AE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(AE_CAMERA_MODULES
    native_camera native_camera_collision native_camera_follow native_camera_host native_camera_render
    native_camera_bank native_camera_regions native_camera_select native_camera_blend native_camera_focus
    native_camera_script native_camera_motion native_camera_molten native_camera_lifecycle native_camera_stage)
set(AE_CAMERA_SOURCES)
foreach(m IN LISTS AE_CAMERA_MODULES)
    list(APPEND AE_CAMERA_SOURCES ${AE_SRC}/camera/${m}.c)
endforeach()

add_executable(native_viewer
    ${AE_SRC}/app/native_viewer.c
    ${AE_SRC}/app/native_audio_output.c
    ${AE_SRC}/app/native_input.c
    ${AE_SRC}/app/native_player_host.c
    ${AE_SRC}/render/native_actor.c
    ${AE_SRC}/render/native_sky.c
    ${AE_SRC}/collision/native_ground.c
    ${AE_SRC}/collision/native_collision.c
    ${AE_SRC}/audio/sp_player.c
    ${AE_SRC}/ui/native_ui.c
    ${AE_SRC}/text/tl_text.c
    ${AE_SRC}/camera/native_camera_math.c
    ${AE_SRC}/indicators/native_indicators.c
    ${AE_SRC}/indicators/native_indicator_runtime.c
    ${AE_SRC}/indicators/native_indicator_trigger.c
    ${AE_SRC}/indicators/native_indicator_world.c
    ${AE_SRC}/player/native_player.c
    ${AE_SRC}/player/native_player_body.c
    ${AE_SRC}/player/native_player_volume.c
    ${AE_SRC}/player/native_player_pad.c
    ${AE_SRC}/player/native_gadget_slots.c
    ${AE_SRC}/core/ae_ram.c
    ${AE_SRC}/core/ae_pad.c
    ${AE_CAMERA_SOURCES})
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tools/codex/viewer_game_sources.txt" AE_GAME_RELATIVE)
get_target_property(AE_EXISTING native_viewer SOURCES)
foreach(rel IN LISTS AE_GAME_RELATIVE)
    set(candidate "${CMAKE_CURRENT_SOURCE_DIR}/${rel}")
    if(NOT candidate IN_LIST AE_EXISTING)
        target_sources(native_viewer PRIVATE "${candidate}")
    endif()
endforeach()
target_sources(native_viewer PRIVATE ${AE_SRC}/app/viewer_game.c)
target_compile_definitions(native_viewer PRIVATE AE_NATIVE_PLAYER)
target_link_libraries(native_viewer PRIVATE SDL3::SDL3 OpenGL::GL)
if(NOT MSVC)
    target_compile_options(native_viewer PRIVATE -Wall -Wextra -Werror $<$<CONFIG:Release>:-O2>)
    target_link_libraries(native_viewer PRIVATE m)
endif()
if(MINGW)
    target_link_options(native_viewer PRIVATE -static-libgcc)
endif()
if(WIN32)
    add_custom_command(TARGET native_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:native_viewer>)
endif()
