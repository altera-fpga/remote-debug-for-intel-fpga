cmake_minimum_required(VERSION 3.11) # Minimum version for FetchContent

option(SW_MODEL "SW Model Unit Test")

option(FPGA_PLATFORM_FORCE_64BIT_MMIO_EMULATION_WITH_32BIT "Option to use 32-bit MMIO for 64-bit MMIO access on certainly PCIe endpoint lacking 64-bit MMIO support" OFF) # Disabled by default
if(FPGA_PLATFORM_FORCE_64BIT_MMIO_EMULATION_WITH_32BIT)
    add_definitions(-DFPGA_PLATFORM_FORCE_64BIT_MMIO_EMULATION_WITH_32BIT)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SW_MODEL_FLAG} -Wall -Wno-unused-function")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L /usr/local/lib -pthread" )

project(remote-debug-for-intel-fpga)

include(FetchContent)

set(IP_ACCESS_API_LIB_GIT_URL "https://github.com/altera-fpga/fpga-ip-access.git" CACHE STRING "URL of the IP Access API for Intel FPGAs library repository")
set(IP_ACCESS_API_LIB_GIT_TAG "main" CACHE STRING "Git tag to use for the IP Access API for Intel FPGAs library repository")
FetchContent_Declare(
  IP_ACCESS_API_LIB
  GIT_REPOSITORY ${IP_ACCESS_API_LIB_GIT_URL}
  GIT_TAG ${IP_ACCESS_API_LIB_GIT_TAG}
)
FetchContent_MakeAvailable(IP_ACCESS_API_LIB)

add_subdirectory(${CMAKE_SOURCE_DIR}/streaming)

#include(version.cmake)

add_custom_target(version
  ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/app_version.h.in
                   -D DST=${CMAKE_BINARY_DIR}/app_version.h
                   -P ${CMAKE_SOURCE_DIR}/version.cmake
  )

file(GLOB main_FILE main.cpp)
add_executable(etherlink ${main_FILE})
target_include_directories(etherlink PRIVATE "$<TARGET_PROPERTY:fpga_ip_access_lib,INTERFACE_INCLUDE_DIRECTORIES>" ${CMAKE_BINARY_DIR})
target_link_libraries(etherlink LINK_PUBLIC fpga_ip_access_lib fpga_ip_access_lib_common streaming )
add_dependencies(etherlink version)
install(TARGETS etherlink DESTINATION bin)
