cmake_minimum_required (VERSION 3.8)
project (vk_async_compute)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

find_package(Vulkan)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

set (CMAKE_CXX_STANDARD 17)

include_directories(${Vulkan_INCLUDE_DIR})

set(ALL_LIBS  ${Vulkan_LIBRARY} )

add_executable(vk_async_compute
        src/main.cpp
        src/vk_utils.cpp
        src/Bitmap.cpp)

set_target_properties(vk_async_compute PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if(WIN32)
    target_link_libraries(vk_async_compute ${ALL_LIBS})
else()
    target_link_libraries(vk_async_compute ${ALL_LIBS} pthread)
endif()

