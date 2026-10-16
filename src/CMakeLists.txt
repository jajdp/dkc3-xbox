# Target UWP (Xbox Series X, dev mode) de DKC3Recomp: presentador Direct3D 11.
# Reusa el motor + el recomp generado + el runner DKC3 del clon del fuente, y cambia la
# capa de host por uwp_host.cpp. NO usa SDL ni OpenGL. Molde tomado de Donkey Kong
# Country 1, que es el primero que corrio en la consola (xbox\spike-xbox-d3d11.md).
#
# Configurar (fuera del arbol, ruta corta por MAX_PATH), con el toolset UWP:
#   cmake -S <este dir> -B <build> -G Ninja ^
#     -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 ^
#     -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
#     -DDKC3_SRC=<ruta a tu copia del proyecto>
#   cmake --build <build> --target dkc3_uwp
cmake_minimum_required(VERSION 3.20)
project(dkc3_uwp C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED DKC3_SRC)
    message(FATAL_ERROR "Pasa -DDKC3_SRC=<ruta a tu copia del proyecto>")
endif()

set(SNESRECOMP_ROOT ${DKC3_SRC}/snesrecomp)
# runner.cmake valida el valor aunque no enlacemos SDL (no llamamos snesrecomp_target_sdl).
set(SNESRECOMP_SDL_BACKEND "SDL2" CACHE STRING "" FORCE)
include(${SNESRECOMP_ROOT}/runner/runner.cmake)

file(GLOB DKC3_GEN CONFIGURE_DEPENDS "${DKC3_SRC}/generated/snesrecomp/*.c")
if(NOT DKC3_GEN)
    message(FATAL_ERROR "No hay fuentes generadas en ${DKC3_SRC}/generated/snesrecomp")
endif()

# Igual que el proyecto: DKC3 trae sus propios keybinds/host_report (headless_host.c).
set(DKC3_ENGINE ${SNESRECOMP_RUNNER_SOURCES})
list(REMOVE_ITEM DKC3_ENGINE
    "${SNESRECOMP_ROOT}/runner/src/keybinds.c"
    "${SNESRECOMP_ROOT}/runner/src/host_report.c")

# Las mismas fuentes del target `dkc3_snesrecomp_headless` del repo, menos headless_main.c.
set(DKC3_RUNNER
    ${DKC3_SRC}/runner/dkc3_game.c
    ${DKC3_SRC}/runner/dkc3_video.c
    ${DKC3_SRC}/runner/dkc3_hdma.c
    ${DKC3_SRC}/runner/input_playback.c
    ${DKC3_SRC}/runner/verified_rom.c)

add_executable(dkc3_uwp WIN32
    ${DKC3_ENGINE}
    ${DKC3_GEN}
    ${DKC3_RUNNER}
    ${DKC3_SRC}/runner/headless_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/uwp_host.cpp)

target_include_directories(dkc3_uwp PRIVATE
    ${DKC3_SRC}/runner
    ${DKC3_SRC}/recomp
    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(dkc3_uwp PRIVATE
    SNESRECOMP_TRACE=0
    SNESRECOMP_REVERSE_DEBUG=0
    SNESRECOMP_EXTERNAL_RAM_ROUTINE_GUARDS=1
    SYSTEM_VOLUME_MIXER_AVAILABLE=0
    _CRT_SECURE_NO_WARNINGS
    DKC3_BUILD_COMMIT="uwp"
    DKC3_BUILD_CONFIG="uwp")

target_compile_options(dkc3_uwp PRIVATE
    $<$<COMPILE_LANGUAGE:C>:/W0>
    $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    $<$<COMPILE_LANGUAGE:CXX>:/bigobj>)

# CRT «store» (lo trae el entorno vcvarsall x64 store + /MD) + OneCore, sin user32
# (el MessageBoxA de rom_image_verify.c se cambia por OutputDebugStringA en el clon).
target_link_libraries(dkc3_uwp PRIVATE
    ${SNESRECOMP_RUNNER_LIBRARIES}
    d3d11 dxgi xaudio2 WindowsApp OneCore)

# El generador Ninja de WindowsStore NO anade /APPCONTAINER solo; sin ese bit el loader
# de UWP rechaza el paquete al activarlo.
target_link_options(dkc3_uwp PRIVATE /APPCONTAINER)

set_target_properties(dkc3_uwp PROPERTIES OUTPUT_NAME DKC3Recomp)
