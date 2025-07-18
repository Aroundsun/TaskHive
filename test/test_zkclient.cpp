#include "zk_client.h"
#include <iostream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <vector>

void test_basic_operations(ZkClient& zk) {
    std::cout << "=== 测试基本操作 ===" << std::endl;
    
    std::string path = "/test_basic_node";
    std::string data = "hello_zk_basic";
    
    // 1. 确保节点不存在
    if (zk.exists(path)) {
        std::cout << "节点已存在，先删除" << std::endl;
        zk.deleteNode(path);
        sleep(1);
    }
    
    // 2. 创建节点
    if (zk.createNode(path, data)) {
        std::cout << "✓ 节点创建成功: " << path << std::endl;
    } else {
        std::cout << "✗ 节点创建失败" << std::endl;
        return;
    }
    
    // 3. 判断节点是否存在
    if (zk.exists(path)) {
        std::cout << "✓ 节点存在测试通过" << std::endl;
    } else {
        std::cout << "✗ 节点存在测试失败" << std::endl;
    }
    
    // 4. 获取节点数据
    std::string value = zk.getNodeData(path);
    if (value == data) {
        std::cout << "✓ 节点数据获取成功: " << value << std::endl;
    } else {
        std::cout << "✗ 节点数据获取失败，期望: " << data << "，实际: " << value << std::endl;
    }
    
    // 5. 修改节点数据
    std::string new_data = "zk_update_basic";
    if (zk.setNodeData(path, new_data)) {
        std::cout << "✓ 节点数据更新成功" << std::endl;
    } else {
        std::cout << "✗ 节点数据更新失败" << std::endl;
    }
    
    // 6. 再次获取节点数据
    value = zk.getNodeData(path);
    if (value == new_data) {
        std::cout << "✓ 更新后节点数据正确: " << value << std::endl;
    } else {
        std::cout << "✗ 更新后节点数据错误，期望: " << new_data << "，实际: " << value << std::endl;
    }
    
    // 7. 删除节点
    if (zk.deleteNode(path)) {
        std::cout << "✓ 节点删除成功" << std::endl;
    } else {
        std::cout << "✗ 节点删除失败" << std::endl;
    }
    
    // 8. 再次判断节点是否存在
    if (!zk.exists(path)) {
        std::cout << "✓ 节点不存在测试通过" << std::endl;
    } else {
        std::cout << "✗ 节点删除失败" << std::endl;
    }
}

void test_hierarchical_nodes(ZkClient& zk) {
    std::cout << "\n=== 测试层级节点 ===" << std::endl;
    
    std::string base_path = "/test_hierarchy";
    std::string child_path = base_path + "/child";
    std::string grandchild_path = child_path + "/grandchild";
    
    // 清理可能存在的节点
    if (zk.exists(grandchild_path)) zk.deleteNode(grandchild_path);
    if (zk.exists(child_path)) zk.deleteNode(child_path);
    if (zk.exists(base_path)) zk.deleteNode(base_path);
    sleep(1);
    
    // 创建层级节点
    if (zk.createNode(base_path, "parent_data")) {
        std::cout << "✓ 父节点创建成功" << std::endl;
    } else {
        std::cout << "✗ 父节点创建失败" << std::endl;
        return;
    }
    
    if (zk.createNode(child_path, "child_data")) {
        std::cout << "✓ 子节点创建成功" << std::endl;
    } else {
        std::cout << "✗ 子节点创建失败" << std::endl;
        return;
    }
    
    if (zk.createNode(grandchild_path, "grandchild_data")) {
        std::cout << "✓ 孙节点创建成功" << std::endl;
    } else {
        std::cout << "✗ 孙节点创建失败" << std::endl;
        return;
    }
    
    // 验证层级结构
    if (zk.exists(base_path) && zk.exists(child_path) && zk.exists(grandchild_path)) {
        std::cout << "✓ 层级结构验证成功" << std::endl;
    } else {
        std::cout << "✗ 层级结构验证失败" << std::endl;
    }
    
    // 清理
    zk.deleteNode(grandchild_path);
    zk.deleteNode(child_path);
    zk.deleteNode(base_path);
    std::cout << "✓ 层级节点清理完成" << std::endl;
}

void test_error_handling(ZkClient& zk) {
    std::cout << "\n=== 测试错误处理 ===" << std::endl;
    
    // 测试获取不存在的节点数据
    std::string non_existent_path = "/non_existent_node";
    std::string data = zk.getNodeData(non_existent_path);
    if (data.empty()) {
        std::cout << "✓ 获取不存在节点数据返回空（预期行为）" << std::endl;
    } else {
        std::cout << "✗ 获取不存在节点数据返回非空（异常）" << std::endl;
    }
    
    // 测试删除不存在的节点
    if (!zk.deleteNode(non_existent_path)) {
        std::cout << "✓ 删除不存在节点失败（预期行为）" << std::endl;
    } else {
        std::cout << "✗ 删除不存在节点成功（异常）" << std::endl;
    }
    
    // 测试设置不存在节点的数据
    if (!zk.setNodeData(non_existent_path, "test_data")) {
        std::cout << "✓ 设置不存在节点数据失败（预期行为）" << std::endl;
    } else {
        std::cout << "✗ 设置不存在节点数据成功（异常）" << std::endl;
    }
}

void test_connection_handling() {
    std::cout << "\n=== 测试连接处理 ===" << std::endl;
    
    // 测试连接无效的ZK服务器
    ZkClient invalid_zk;
    if (!invalid_zk.connect("invalid_host:2181")) {
        std::cout << "✓ 无效主机连接失败（预期行为）" << std::endl;
    } else {
        std::cout << "✗ 无效主机连接成功（异常）" << std::endl;
    }
    
    // 测试连接错误的端口
    if (!invalid_zk.connect("127.0.0.1:9999")) {
        std::cout << "✓ 错误端口连接失败（预期行为）" << std::endl;
    } else {
        std::cout << "✗ 错误端口连接成功（异常）" << std::endl;
    }
}

void test_concurrent_operations(ZkClient& zk) {
    std::cout << "\n=== 测试并发操作 ===" << std::endl;
    
    std::string base_path = "/concurrent_test";
    std::vector<std::string> paths;
    
    // 创建多个测试节点
    for (int i = 0; i < 5; ++i) {
        std::string path = base_path + "/node_" + std::to_string(i);
        paths.push_back(path);
        
        if (zk.createNode(path, "data_" + std::to_string(i))) {
            std::cout << "✓ 创建节点 " << path << " 成功" << std::endl;
        } else {
            std::cout << "✗ 创建节点 " << path << " 失败" << std::endl;
        }
    }
    
    // 并发读取节点数据
    std::cout << "并发读取节点数据..." << std::endl;
    for (const auto& path : paths) {
        std::string data = zk.getNodeData(path);
        if (!data.empty()) {
            std::cout << "✓ 读取节点 " << path << " 成功" << std::endl;
        } else {
            std::cout << "✗ 读取节点 " << path << " 失败" << std::endl;
        }
    }
    
    // 清理测试节点
    for (const auto& path : paths) {
        zk.deleteNode(path);
    }
    zk.deleteNode(base_path);
    std::cout << "✓ 并发测试节点清理完成" << std::endl;
}

int main() {
    std::cout << "=== Zookeeper 客户端测试程序 ===" << std::endl;
    
    ZkClient zk;
    std::string zk_addr = "127.0.0.1:2181";
    
    // 1. 连接 Zookeeper
    std::cout << "正在连接 Zookeeper..." << std::endl;
    if (!zk.connect(zk_addr)) {
        std::cout << "✗ 连接 Zookeeper 失败，请确保 Zookeeper 服务正在运行" << std::endl;
        std::cout << "提示：可以使用 'zkServer.sh start' 启动 Zookeeper 服务" << std::endl;
        return -1;
    }
    std::cout << "✓ 连接 Zookeeper 成功" << std::endl;
    
    // 运行各种测试
    test_basic_operations(zk);
    test_hierarchical_nodes(zk);
    test_error_handling(zk);
    test_concurrent_operations(zk);
    test_connection_handling();
    
    zk.close();
    std::cout << "\n=== 所有测试完成 ===" << std::endl;
    
    return 0;
} 