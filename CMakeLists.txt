cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

# 编译警告
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# 添加构建测试选项，默认为ON
option(BUILD_TESTS "Build test programs" OFF)

# 设置统一输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 查找必要的库
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# 构建函数库动态库
add_library(myfuncs SHARED src/function_lib.cpp)
target_link_libraries(myfuncs PRIVATE ${JSONCPP_LIBRARIES} pthread dl)
target_include_directories(myfuncs PRIVATE ${JSONCPP_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/proto)

# 构建调度器程序
add_executable(scheduler src/scheduler_main.cpp src/scheduler.cpp src/redis_client.cpp src/zk_client.cpp proto/task.pb.cc) 
target_link_libraries(scheduler PRIVATE ${JSONCPP_LIBRARIES} hiredis zookeeper_mt protobuf rabbitmq pthread dl)
target_include_directories(scheduler PRIVATE ${JSONCPP_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/proto)

# 构建工作器程序
add_executable(worker src/worker_main.cpp src/worker.cpp src/redis_client.cpp src/zk_client.cpp src/function_lib.cpp proto/task.pb.cc)
target_link_libraries(worker PRIVATE ${JSONCPP_LIBRARIES} hiredis zookeeper_mt protobuf rabbitmq myfuncs pthread dl)
target_include_directories(worker PRIVATE ${JSONCPP_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/proto)

# 构建客户端程序
add_executable(client src/client_main.cpp src/client.cpp src/redis_client.cpp src/zk_client.cpp proto/task.pb.cc)
target_link_libraries(client PRIVATE ${JSONCPP_LIBRARIES} hiredis protobuf pthread dl zookeeper_mt)
target_include_directories(client PRIVATE ${JSONCPP_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/proto)

# 根据BUILD_TESTS选项决定是否构建测试程序
if(BUILD_TESTS)
    message(STATUS "Building test programs...")
    
    # 构建Redis测试程序
    add_executable(test_redis test/test_redis.cpp src/redis_client.cpp)
    target_link_libraries(test_redis PRIVATE hiredis pthread dl)
    target_include_directories(test_redis PRIVATE ${PROJECT_SOURCE_DIR}/include)
    
    # 构建ZK测试程序
    add_executable(test_zkclient test/test_zkclient.cpp src/zk_client.cpp)
    target_link_libraries(test_zkclient PRIVATE zookeeper_mt pthread dl)
    target_include_directories(test_zkclient PRIVATE ${PROJECT_SOURCE_DIR}/include)
    
    # 构建消息队列测试程序
    add_executable(test_message_queue test/test_message_queue.cpp proto/task.pb.cc)
    target_link_libraries(test_message_queue PRIVATE ${JSONCPP_LIBRARIES} protobuf rabbitmq pthread dl)
    target_include_directories(test_message_queue PRIVATE ${JSONCPP_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/proto)
    
    # 创建测试目标组
    add_custom_target(tests DEPENDS test_redis test_zkclient test_message_queue)
    
    message(STATUS "Test programs will be built")
else()
    message(STATUS "Skipping test programs build")
endif()

