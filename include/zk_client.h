#pragma once
#include <string>
#include <vector>
#include <iostream>
extern "C" {
#include <zookeeper/zookeeper.h>
}
#include<shared_mutex>

class ZkClient
{
public:
    ZkClient();
    ~ZkClient();

    // 连接到Zookeeper服务器
    bool connect(const std::string &host_port, int timeout = 30000);

    // 创建节点
    bool createNode(const std::string &path, const std::string &data, int flags = 0);

    // 设置节点数据
    bool setNodeData(const std::string &path, const std::string &data);

    // 获取节点数据
    std::string getNodeData(const std::string &path);

    // 删除节点
    bool deleteNode(const std::string &path);

    // 获取这个path 下所有节点
    std::vector<std::string> getAllNode(const std::string &path);

    // 判断节点是否存在
    bool exists(const std::string &path);

    // 关闭连接
    void close();

private:
    // zk 事件监视器
    static void watcher(zhandle_t *zkH, int type, int state, const char *path, void *watcherCtx)
    {
        if (type == ZOO_SESSION_EVENT) //有事件触发
        {

            if (state == ZOO_CONNECTED_STATE) //连接成功
            {
                std::cout << "连接成功" << std::endl;
            }
            else if (state == ZOO_NOTCONNECTED_STATE) //连接失败
            {
                std::cout << "连接失败" << std::endl;
            }
            else if (state == ZOO_EXPIRED_SESSION_STATE) //回话过期
            {
                std::cout << "回话过期" << std::endl;
                zookeeper_close(zkH);
            }
        }
    }
private:
    zhandle_t *zh_;
    std::shared_mutex mtx_;

};