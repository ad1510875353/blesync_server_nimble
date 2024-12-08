# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# 这句话的意思是 将例程中的nimble_peripheral_utils作为组件引入进来
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/bluetooth/nimble/common/nimble_peripheral_utils)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blesync_server_nimble)
