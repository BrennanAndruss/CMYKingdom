# Collect engine source files
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/implementations.cpp"
    "${CMAKE_SOURCE_DIR}/external/glad/src/*.c"
)

# Collect engine headers (for IDE support)
file(GLOB_RECURSE ENGINE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"    
)

add_library(engine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Define project root directory (for cross-IDE compatibility)
target_compile_definitions(engine PUBLIC
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}/"
    ENGINE_ROOT="${CMAKE_SOURCE_DIR}/engine/"
)

# Include directories
target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external/glad/include
    ${CMAKE_SOURCE_DIR}/external/miniaudio
    ${CMAKE_SOURCE_DIR}/external/stb_image
    ${CMAKE_SOURCE_DIR}/external/tiny_obj_loader
    ${bullet_SOURCE_DIR}/src
)

target_include_directories(engine PRIVATE
    ${CMAKE_SOURCE_DIR}/game
)

set(ASSIMP_LINK_TARGET assimp)
if (TARGET assimp::assimp)
    set(ASSIMP_LINK_TARGET assimp::assimp)
endif()

# Link dependencies
target_link_libraries(engine PUBLIC
    glfw
    glm
    OpenGL::GL
    imgui_static
    BulletDynamics
    BulletCollision
    LinearMath
    ${ASSIMP_LINK_TARGET}
)

if (APPLE)
    target_link_libraries(engine PUBLIC
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()