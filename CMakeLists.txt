cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

#添加头文件
include_directories(./include)
include_directories(./proto)

#添加源文件
#file(GLOB SOURCES "src/*.cpp")

#添加proto文件
file(GLOB PROTO_FILES "proto/*.proto")

#添加proto文件到可执行文件
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND protoc --cpp_out=./src ${PROTO_FILES}
)

#添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

