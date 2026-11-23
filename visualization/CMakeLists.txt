if(WIN32)
    set(glfw_default "C:/libs/glfw-3.4/glfw-3.4")
    set(glad_default "C:/libs/glad")
else()
    set(glfw_default "")
    set(glad_default "")
endif()
set(GLFW_SOURCE_DIR "${glfw_default}" CACHE PATH "GLFW source directory")
set(GLAD_ROOT "${glad_default}" CACHE PATH "GLAD 1 generated source directory")

if(NOT EXISTS "${GLFW_SOURCE_DIR}/CMakeLists.txt" OR
   NOT EXISTS "${GLFW_SOURCE_DIR}/include/GLFW/glfw3.h")
    message(FATAL_ERROR "Set GLFW_SOURCE_DIR to the root of your GLFW source tree.")
endif()
if(NOT EXISTS "${GLAD_ROOT}/include/glad/glad.h" OR
   NOT EXISTS "${GLAD_ROOT}/src/glad.c")
    message(FATAL_ERROR "Set GLAD_ROOT to a GLAD 1 tree with include/glad/glad.h and src/glad.c.")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
set(GLFW_INSTALL OFF CACHE BOOL "Install GLFW")
add_subdirectory("${GLFW_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)

add_library(nbody_glad STATIC "${GLAD_ROOT}/src/glad.c")
target_include_directories(nbody_glad PUBLIC "${GLAD_ROOT}/include")
target_link_libraries(nbody_glad PRIVATE ${CMAKE_DL_LIBS})

add_executable(nbody_viewer main.cpp csv.cpp playback.cpp camera.cpp renderer.cpp)
target_compile_definitions(nbody_viewer PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(nbody_viewer PRIVATE glfw nbody_glad nbody_view_settings)
