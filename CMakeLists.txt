cmake_minimum_required(VERSION 3.25)
project(AuraUltimate LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. PLATFORM CONFIG
if(APPLE)
    add_definitions(-DAURA_PLATFORM_MACOS -DAURA_USE_METAL)
    set(GRAPHICS_LIBS "-framework Metal -framework MetalKit -framework Cocoa -framework QuartzCore")
    set(SYSTEM_LIBS "-framework AudioToolbox -framework AudioUnit -framework CoreAudio")
else()
    add_definitions(-DAURA_PLATFORM_GENERIC -DAURA_USE_VULKAN)
    set(GRAPHICS_LIBS "vulkan")
endif()

# 2. METAL SHADER COMPILATION (Professional Mac Workflow)
if(APPLE)
    set(SHADER_SRC "src/graphics/platform/metal_ui.metal")
    set(SHADER_LIB "${CMAKE_CURRENT_BINARY_DIR}/default.metallib")
    
    add_custom_command(
        OUTPUT ${SHADER_LIB}
        COMMAND xcrun -sdk macosx metal -c ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SRC} -o ${CMAKE_CURRENT_BINARY_DIR}/metal_ui.air
        COMMAND xcrun -sdk macosx metallib ${CMAKE_CURRENT_BINARY_DIR}/metal_ui.air -o ${SHADER_LIB}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling Aura Metal Shaders..."
    )
    
    # Ensure the library is bundled into the .app/Contents/Resources
    set_source_files_properties(${SHADER_LIB} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

# 3. SOURCE FILES
set(SOURCES
    src/ui/platform/macos_window.mm
    src/graphics/platform/metal_kernel.mm
    ${SHADER_LIB} # Include the compiled metallib here
    src/dsp/analysis/analyzer_8khz.cpp
    src/dsp/mixing/fade_enveloper.cpp
    src/dsp/synthesis/aura_sampler_pro.cpp
    src/core/engine/engine_clock.cpp
    src/io/coreaudio_driver.cpp
    src/driver/macos_driver.mm
)

# 4. EXECUTABLE
add_executable(AuraUltimate MACOSX_BUNDLE ${SOURCES})

# 5. INCLUDE DIRECTORIES
target_include_directories(AuraUltimate PRIVATE 
    src/
    src/core/engine/
    src/graphics/
    src/graphics/ui_components/
    src/external/
)

# 6. LINK LIBRARIES
target_link_libraries(AuraUltimate PRIVATE ${GRAPHICS_LIBS} ${SYSTEM_LIBS})

# 7. APP BUNDLE
if(APPLE)
    set_target_properties(AuraUltimate PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.aura.daoultimate"
        MACOSX_BUNDLE_BUNDLE_NAME "AuraUltimate"
    )
endif()
