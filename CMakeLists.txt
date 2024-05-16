# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/esp-aws-iot")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})
project(${ProjectId})

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/certs/aws_root_ca_pem" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/certs/certificate_pem_crt" TEXT)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "main/certs/private_pem_key" TEXT)
