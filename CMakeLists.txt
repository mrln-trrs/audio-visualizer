cmake_minimum_required(VERSION 3.20)
project(audio_visualizer VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Las bibliotecas incluidas son de 64 bits. Configura con: cmake -B build -S . -A x64")
endif()

# -----------------------------------------------------------------------------
# Rutas de las bibliotecas versionadas en el repositorio
# -----------------------------------------------------------------------------
set(THIRD_PARTY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(GLFW_DIR   "${THIRD_PARTY_ROOT}/glfw-3.4.bin.WIN64")
set(GLEW_DIR   "${THIRD_PARTY_ROOT}/glew-2.1.0")
set(FFTW_DIR   "${THIRD_PARTY_ROOT}/fftw-3.3.5-dll64")
set(JSON_DIR   "${THIRD_PARTY_ROOT}/json-develop")
set(IMGUI_DIR  "${THIRD_PARTY_ROOT}/imgui-1.91.5")

foreach(dir IN ITEMS ${GLFW_DIR} ${GLEW_DIR} ${FFTW_DIR} ${JSON_DIR} ${IMGUI_DIR})
    if(NOT EXISTS "${dir}")
        message(FATAL_ERROR "Falta la carpeta de biblioteca ${dir}. Debe estar versionada en el repositorio.")
    endif()
endforeach()

# -----------------------------------------------------------------------------
# Dear ImGui (vendorizado en imgui-1.91.5/, sin descargas en tiempo de configuración)
# -----------------------------------------------------------------------------
add_library(imgui_lib STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLFW_DIR}/include
)

# -----------------------------------------------------------------------------
# Ejecutable
# -----------------------------------------------------------------------------
set(PROJECT_SOURCES
    main.cpp
    audio-capture.h
    audio-capture.cpp
    audio-processing.h
    audio-processing.cpp
    renderer.h
    renderer.cpp
    config.h
    config.cpp
    common.h
)

# Aplicación de ventana (sin consola) con main() clásico como punto de entrada.
add_executable(audio-visualizer WIN32 ${PROJECT_SOURCES})

if(MSVC)
    target_compile_options(audio-visualizer PRIVATE /W4 /utf-8 /EHsc /permissive- /Zc:__cplusplus)
    target_compile_options(imgui_lib PRIVATE /W3 /utf-8 /EHsc)
    target_link_options(audio-visualizer PRIVATE /ENTRY:mainCRTStartup)
endif()

target_include_directories(audio-visualizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLFW_DIR}/include
    ${GLEW_DIR}/include
    ${FFTW_DIR}
    ${JSON_DIR}/include
)

target_link_directories(audio-visualizer PRIVATE
    ${GLFW_DIR}/lib-vc2022
    ${GLEW_DIR}/lib/Release/x64
    ${FFTW_DIR}
)

target_link_libraries(audio-visualizer PRIVATE
    imgui_lib
    glfw3dll.lib
    glew32.lib
    libfftw3f-3.lib
    opengl32.lib
    winmm.lib
    ole32.lib
    avrt.lib
    uuid.lib
)

# -----------------------------------------------------------------------------
# Post-build: DLL, shaders y config.json junto al ejecutable
# -----------------------------------------------------------------------------
add_custom_command(TARGET audio-visualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFTW_DIR}/libfftw3f-3.dll"
        "$<TARGET_FILE_DIR:audio-visualizer>/libfftw3f-3.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLEW_DIR}/bin/Release/x64/glew32.dll"
        "$<TARGET_FILE_DIR:audio-visualizer>/glew32.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLFW_DIR}/lib-vc2022/glfw3.dll"
        "$<TARGET_FILE_DIR:audio-visualizer>/glfw3.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:audio-visualizer>/config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:audio-visualizer>/shaders"
    COMMENT "Copiando DLL, shaders y config.json a la carpeta del ejecutable"
)
