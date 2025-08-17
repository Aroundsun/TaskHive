#pragma once

#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <pthread.h>
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <functional>

#ifndef TEST_LOGGER

#if BACKUP_LOG
using func = std::function<void(const std::string &)>;
constexpr int backlog = 32; // 最大连接数


class TcpServer;
// 客户端信息
class ClientInfo
{
public:
    ClientInfo(int client_socket, const std::string &client_ip, int client_port, TcpServer *server)
        : client_socket_(client_socket), client_ip_(client_ip), client_port_(client_port), server_(server) {}

    std::string client_ip_port_str() const
    {
        return client_ip_ + ":" + std::to_string(client_port_);
    }
    // 获取客户端套接字
    int client_socket() const
    {
        return client_socket_;
    }
    // 获取客户端ip
    std::string client_ip() const
    {
        return client_ip_;
    }
    // 获取客户端端口
    int client_port() const
    {
        return client_port_;
    }
    // 获取服务器指针
    TcpServer *server() const
    {
        return server_;
    }
    ~ClientInfo()
    {
        close(client_socket_);
    }

private:
    // 客户端套接字
    int client_socket_;
    // 客户端ip
    std::string client_ip_;
    // 客户端端口
    int client_port_;
    // 服务器指针
    TcpServer *server_;
};

class TcpServer
{
public:
    TcpServer(uint16_t port, func cb)
        : port_(port), callback_(cb) {}

    void init_service()
    {
        // 创建
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ < 0)
        {
            std::cerr << "socket creation failed: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
        // 设置端口复用
        struct sockaddr_in local;
        local.sin_family = AF_INET;                // IPV4
        local.sin_port = htons(port_); // 绑定地址为指定端口
        local.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定地址在本地
        
        std::cout<< "Server is listening on ip:port: " <<local.sin_addr.s_addr<<":"<< port_ << std::endl;

        // 绑定
        if (bind(server_socket_, (struct sockaddr *)&local, sizeof(local)) < 0)
        {
            std::cout << __FILE__ << __LINE__ << "bind socket error" << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
        // 监听
        if (listen(server_socket_, backlog) < 0)
        {
            std::cout << __FILE__ << __LINE__ << "listen socket error" << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // 线程入口函数
    static void *thread_func(void *arg)
    {
        // 防止在线程在被创建时被阻塞
        pthread_detach(pthread_self());
        ClientInfo *cli = static_cast<ClientInfo *>(arg);
        std::string client_info = cli->client_ip_port_str();

        cli->server()->service(cli->client_socket(), std::move(client_info));
        delete cli; // 释放客户端信息对象
        // 线程结束时自动清理资源
        pthread_exit(nullptr); // 线程结束
    }

    void start_service()
    {
        while (true)
        {
            // 接收连接
            std::cout << "Waiting for new connections..." << std::endl;
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_sock = accept(server_socket_, (struct sockaddr *)&client_addr, &addr_len);
            if (client_sock < 0)
            {
                std::cout << __FILE__ << __LINE__ << "accept socket error" << strerror(errno) << std::endl;
                continue; // 继续等待下一个连接
            }
            // 获取客户端信息
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            int client_port = ntohs(client_addr.sin_port);

            //DEBUG
            std::cout << "New connection from " << client_ip << ":" << client_port << std::endl;


            // 多个线程提供服务
            ClientInfo *client_info = new ClientInfo(client_sock, client_ip, client_port, this);
            pthread_t tid;
            pthread_create(&tid, nullptr, thread_func, static_cast<void *>(client_info));
        }
    }

    void service(int client_sock, std::string &&client_info)
    {
        char buf[1024] = {0};

        int res = read(client_sock, buf, 1024);
        if (res == -1)
        {
            std::cout << __FILE__ << __LINE__ << "read error" << strerror(errno) << std::endl;
            perror("NULL");
        }
        else if (res > 0)
        {
            buf[res] = '\0'; // 确保字符串以null结尾
            std::string msg(buf);
            callback_(msg); // 调用回调函数处理消息
        }
    }
    ~TcpServer() = default;

private:
    int server_socket_; // 服务器套接字
    uint16_t port_;     // 服务器端口
    func callback_;     // 回调函数
};
#endif // BACKUP_LOG


#endif // TEST_LOGGER