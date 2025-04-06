cmake_minimum_required(VERSION 3.30)
project(CodotakuGameEngine)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
function(include_dependency libName gitURL gitTAG)
    FetchContent_Declare(${libName}
            GIT_REPOSITORY ${gitURL}
            GIT_TAG ${gitTAG}
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(${libName})
endfunction()

## Dependencies
set(LIBS)

# SDL
include_dependency(SDL3 https://github.com/libsdl-org/SDL release-3.2.8)
list(APPEND LIBS SDL3::SDL3)

# SDL_image
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE) # Requires Perl and Nasm for libaom (AVIF image format)
include_dependency(SDL3_image https://github.com/libsdl-org/SDL_image release-3.2.4)
list(APPEND LIBS SDL3_image::SDL3_image)

# glm
include_dependency(glm https://github.com/g-truc/glm 1.0.1)
list(APPEND LIBS glm::glm-header-only)

# assimp
include_dependency(assimp https://github.com/assimp/assimp v5.4.3)
list(APPEND LIBS assimp::assimp)

## Executables
add_executable(${PROJECT_NAME} src/main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

# Copy assets
file(COPY Content DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})