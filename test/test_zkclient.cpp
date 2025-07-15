#include "zk_client.h"
#include <iostream>
#include <unistd.h>

int main() {
    ZkClient zk;
    std::string zk_addr = "127.0.0.1:2181";
    std::string path = "/test_node";
    std::string data = "hello_zk";

    // 1. 连接 Zookeeper
    if (!zk.connect(zk_addr)) {
        std::cout << "连接 Zookeeper 失败" << std::endl;
        return -1;
    }
    std::cout << "连接 Zookeeper 成功" << std::endl;

    // 2. 确保节点不存在
    if (zk.exists(path)) {
        std::cout << "节点已存在，先删除" << std::endl;
        zk.deleteNode(path);
        sleep(1);
    }

    // 3. 创建节点
    if (zk.createNode(path, data)) {
        std::cout << "节点创建成功: " << path << std::endl;
    } else {
        std::cout << "节点创建失败" << std::endl;
        return -1;
    }

    // 4. 判断节点是否存在
    if (zk.exists(path)) {
        std::cout << "节点存在测试通过" << std::endl;
    } else {
        std::cout << "节点存在测试失败" << std::endl;
    }

    // 5. 获取节点数据
    std::string value = zk.getNodeData(path);
    std::cout << "节点数据: " << value << std::endl;

    // 6. 修改节点数据
    std::string new_data = "zk_update";
    if (zk.setNodeData(path, new_data)) {
        std::cout << "节点数据更新成功" << std::endl;
    }

    // 7. 再次获取节点数据
    value = zk.getNodeData(path);
    std::cout << "更新后节点数据: " << value << std::endl;

    // 8. 删除节点
    if (zk.deleteNode(path)) {
        std::cout << "节点删除成功" << std::endl;
    }

    // 9. 再次判断节点是否存在
    if (!zk.exists(path)) {
        std::cout << "节点不存在测试通过" << std::endl;
    } else {
        std::cout << "节点删除失败" << std::endl;
    }

    zk.close();
    return 0;
} 