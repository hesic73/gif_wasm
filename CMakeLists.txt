cmake_minimum_required(VERSION 3.13)
project(gif_wasm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
add_executable(gif_encoder
    src/bindings.cpp
)

# Include directories - add vendor submodule
target_include_directories(gif_encoder PRIVATE 
    src
    vendor/msf_gif
)

# Emscripten-specific settings
if(EMSCRIPTEN)
    set_target_properties(gif_encoder PROPERTIES
        SUFFIX ".js"
    )
    
    target_compile_options(gif_encoder PRIVATE
        -O3
        -flto
    )
    
    # Emscripten link flags
    target_link_options(gif_encoder PRIVATE
        -O3
        -flto
        --bind
        "SHELL:-s WASM=1"
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME=createGifEncoderModule"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:-s ENVIRONMENT=web"
        "SHELL:-s NO_EXIT_RUNTIME=1"
    )
endif()
