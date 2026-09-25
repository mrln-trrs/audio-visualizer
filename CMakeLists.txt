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
    src/analysis/analysis_thread.cpp
    src/analysis/band_metrics.cpp
    src/analysis/band_synthesizer.cpp
    src/analysis/window_function.cpp
    src/app/application.cpp
    src/audio/capture_session.cpp
    src/audio/capture_thread.cpp
    src/audio/device_enumerator.cpp
    src/audio/sample_format.cpp
    src/core/band_layout.cpp
    src/core/config.cpp
    src/core/types.cpp
    src/main.cpp
    src/platform/window_effects.cpp
    src/render/band_waveform_texture.cpp
    src/render/bar_spectrum.cpp
    src/render/data_textures.cpp
    src/render/frame_limiter.cpp
    src/render/fullscreen_quad.cpp
    src/render/gl_window.cpp
    src/render/modes/band_meters_mode.cpp
    src/render/modes/bars_mode.cpp
    src/render/modes/radial_mode.cpp
    src/render/modes/stacked_oscilloscope_mode.cpp
    src/render/modes/waterfall_mode.cpp
    src/render/modes/waveform_mode.cpp
    src/render/panel_material.cpp
    src/render/post_process.cpp
    src/render/renderer.cpp
    src/render/shader_program.cpp
    src/render/spectrum_dynamics.cpp
    src/ui/hud.cpp
    src/ui/telemetry.cpp
    src/ui/theme.cpp
    src/analysis/analysis_thread.h
    src/analysis/band_metrics.h
    src/analysis/band_synthesizer.h
    src/analysis/window_function.h
    src/app/application.h
    src/audio/capture_session.h
    src/audio/capture_thread.h
    src/audio/device_enumerator.h
    src/audio/sample_format.h
    src/core/analysis_frame.h
    src/core/band_layout.h
    src/core/config.h
    src/core/constants.h
    src/core/shared_state.h
    src/core/triple_buffer.h
    src/core/types.h
    src/platform/window_effects.h
    src/render/band_waveform_texture.h
    src/render/bar_spectrum.h
    src/render/data_textures.h
    src/render/embedded_shaders.h
    src/render/frame_limiter.h
    src/render/fullscreen_quad.h
    src/render/gl_window.h
    src/render/modes/band_meters_mode.h
    src/render/modes/bars_mode.h
    src/render/modes/radial_mode.h
    src/render/modes/stacked_oscilloscope_mode.h
    src/render/modes/visual_mode.h
    src/render/modes/waterfall_mode.h
    src/render/modes/waveform_mode.h
    src/render/panel_material.h
    src/render/post_process.h
    src/render/renderer.h
    src/render/shader_program.h
    src/render/spectrum_dynamics.h
    src/ui/hud.h
    src/ui/motion.h
    src/ui/panel_material.h
    src/ui/telemetry.h
    src/ui/theme.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Fuentes" FILES ${PROJECT_SOURCES})

# Aplicación de ventana (sin consola) con main() clásico como punto de entrada.
add_executable(audio-visualizer WIN32 ${PROJECT_SOURCES})

if(MSVC)
    target_compile_options(audio-visualizer PRIVATE /W4 /utf-8 /EHsc /permissive- /Zc:__cplusplus)
    target_compile_options(imgui_lib PRIVATE /W3 /utf-8 /EHsc)
    target_link_options(audio-visualizer PRIVATE /ENTRY:mainCRTStartup)
endif()

target_include_directories(audio-visualizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
# Pruebas numéricas (sin audio ni ventana): ctest --test-dir build -C Release
# -----------------------------------------------------------------------------
enable_testing()
add_executable(band_metrics_test
    tests/band_metrics_test.cpp
    src/core/band_layout.cpp
    src/analysis/window_function.cpp
    src/analysis/band_metrics.cpp
)
target_include_directories(band_metrics_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${FFTW_DIR})
target_link_directories(band_metrics_test PRIVATE ${FFTW_DIR})
target_link_libraries(band_metrics_test PRIVATE libfftw3f-3.lib)
if(MSVC)
    target_compile_options(band_metrics_test PRIVATE /W4 /utf-8 /EHsc /permissive-)
endif()
add_custom_command(TARGET band_metrics_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFTW_DIR}/libfftw3f-3.dll" "$<TARGET_FILE_DIR:band_metrics_test>/libfftw3f-3.dll")
add_test(NAME band_metrics COMMAND band_metrics_test)

add_executable(band_synthesis_test
    tests/band_synthesis_test.cpp
    src/core/band_layout.cpp
    src/analysis/window_function.cpp
    src/analysis/band_synthesizer.cpp
)
target_include_directories(band_synthesis_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${FFTW_DIR})
target_link_directories(band_synthesis_test PRIVATE ${FFTW_DIR})
target_link_libraries(band_synthesis_test PRIVATE libfftw3f-3.lib)
if(MSVC)
    target_compile_options(band_synthesis_test PRIVATE /W4 /utf-8 /EHsc /permissive-)
endif()
add_custom_command(TARGET band_synthesis_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FFTW_DIR}/libfftw3f-3.dll" "$<TARGET_FILE_DIR:band_synthesis_test>/libfftw3f-3.dll")
add_test(NAME band_synthesis COMMAND band_synthesis_test)

add_executable(motion_test tests/motion_test.cpp)
target_include_directories(motion_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(motion_test PRIVATE /W4 /utf-8 /EHsc /permissive-)
endif()
add_test(NAME motion COMMAND motion_test)

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
