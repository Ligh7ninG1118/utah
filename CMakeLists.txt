cmake_minimum_required(VERSION 3.21)
project(utah LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

file(GLOB_RECURSE UTAH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
list(FILTER UTAH_SOURCES EXCLUDE REGEX ".*/Aftermath/.*")

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/imgui_impl_vulkan.cpp)

add_executable(utah ${UTAH_SOURCES} ${IMGUI_SOURCES})
target_compile_features(utah PRIVATE cxx_std_20)

target_include_directories(utah PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${Stb_INCLUDE_DIR})

target_link_libraries(utah PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    GPUOpen::VulkanMemoryAllocator
    tinyobjloader::tinyobjloader
    fastgltf::fastgltf)

add_custom_target(utah_shaders ALL
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlslCompile.bat
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling HLSL shaders with dxc"
    VERBATIM)
add_dependencies(utah utah_shaders)

set_target_properties(utah PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

option(UTAH_USE_AFTERMATH "Enable NVIDIA Nsight Aftermath GPU crash dumps" OFF)
if(UTAH_USE_AFTERMATH)
    set(AFTERMATH_SDK
        "C:/env/NVIDIA_Nsight_Aftermath_SDK_2025.5.0.25317-windows_x64"
        CACHE PATH "Nsight Aftermath SDK root")
    file(GLOB AFTERMATH_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Render/Aftermath/*.cpp)
    target_sources(utah PRIVATE ${AFTERMATH_SOURCES})
    target_include_directories(utah PRIVATE ${AFTERMATH_SDK}/include)
    target_link_libraries(utah PRIVATE
        ${AFTERMATH_SDK}/lib/x64/GFSDK_Aftermath_Lib.x64.lib)
    target_compile_definitions(utah PRIVATE USE_NSIGHT_AFTERMATH=1)
    add_custom_command(TARGET utah POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AFTERMATH_SDK}/lib/x64/GFSDK_Aftermath_Lib.x64.dll
            $<TARGET_FILE_DIR:utah>
        VERBATIM)
endif()

add_custom_command(TARGET utah POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/models      $<TARGET_FILE_DIR:utah>/models
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/textures    $<TARGET_FILE_DIR:utah>/textures
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaderBin   $<TARGET_FILE_DIR:utah>/shaderBin
    VERBATIM)


target_compile_definitions(utah PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")