file(GLOB all_FILES src/*.c src/*cpp)

add_library(streaming ${all_FILES})
target_include_directories(streaming PUBLIC inc)
target_include_directories(streaming PRIVATE "$<TARGET_PROPERTY:fpga_ip_access_lib,INTERFACE_INCLUDE_DIRECTORIES>")
