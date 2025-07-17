extern "C" {
#define THREADED
#include <zookeeper/zookeeper.h>
}
#include "zk_client.h"
#include <iostream>
#include <string>


ZkClient::ZkClient() : zh_(nullptr) {}

ZkClient::~ZkClient() {
    close();
}


bool ZkClient::connect(const std::string& host_port, int timeout) {
    zh_ = zookeeper_init(host_port.c_str(),watcher,timeout,0,nullptr,0);
    if(!zh_)
        return false;
    return true;
}
/*
节点类型查阅表
ZOO_PERSISTENT	持久节点（默认）
ZOO_EPHEMERAL	临时节点（会话断开即删除）
ZOO_SEQUENCE	顺序节点
ZOO_PERSISTENT_SEQUENTIAL	持久顺序节点
ZOO_EPHEMERAL_SEQUENTIAL   	临时顺序节点
*/

bool ZkClient::createNode(const std::string& path, const std::string& data, int flags) {
    char realpath[128];
    int is_OK = zoo_create(zh_,path.c_str(),
                            data.c_str(),
                            data.size(),
                            &ZOO_OPEN_ACL_UNSAFE,
                            flags,
                            realpath,
                            sizeof(realpath)-1);
    if(is_OK != ZOK)
    {
        return false;
    }
    return true;
}
/*
zoo_set(zhandle_t *zh, const char *path, const char *buffer,
                   int buflen, int version);
*/
bool ZkClient::setNodeData(const std::string& path, const std::string& data) {
    int is_OK = zoo_set(zh_,path.c_str(),data.c_str(),data.size(),-1);
    if(is_OK != ZOK)
        return false;
    return true;
}
/*
ZOOAPI int zoo_get(zhandle_t *zh, const char *path, int watch, char *buffer,
                   int* buffer_len, struct Stat *stat);
*/
std::string ZkClient::getNodeData(const std::string& path) {
    char buffer[1024];
    int buffer_len = sizeof(buffer);
    int ret = zoo_get(zh_, path.c_str(), 0, buffer, &buffer_len, nullptr);
    if (ret != ZOK) {
        // 处理错误
        return "";
    }
    return std::string(buffer, buffer_len);
}

bool ZkClient::exists(const std::string& path) {
    struct Stat stat;
    int ret = zoo_exists(zh_, path.c_str(), 0, &stat);
    return ret == ZOK;
}

bool ZkClient::deleteNode(const std::string& path) {
    int ret = zoo_delete(zh_, path.c_str(), -1);
    return ret == ZOK;
}

void ZkClient::close() {
    if(zh_)
    {
        zookeeper_close(zh_);
        zh_ = nullptr;
    }
} 