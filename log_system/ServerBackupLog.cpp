// 远程备份debug等级以上的日志信息-接收端
#include <string>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <filesystem>
#include "ServerBackupLog.hpp"


#ifndef TEST_LOGGER

#if BACKUP_LOG
static const std::filesystem::path filename = "./backup/logfile.log";

void usage(std::string procgress)
{
    std::cout << "usage error:" << procgress << "port" << std::endl;
}


void backup_log(const std::string &message)
{
    std::ofstream ifs(filename, std::ios::app | std::ios::binary);
    if (!ifs)
    {
        throw std::system_error(errno, std::system_category(),"open " + filename.string());
    }
    ifs << message << '\n';
    if (!ifs)
    {
        throw std::system_error(errno,std::system_category(),"write " + filename.string());
    }
}

int main(int args, char *argv[])
{
    if (args != 2)
    {
        usage(argv[0]);
        perror("usage error");
        exit(-1);
    }

    uint16_t port = atoi(argv[1]);
    std::unique_ptr<TcpServer> tcp(new TcpServer(port, backup_log));

    tcp->init_service();
    tcp->start_service();

    return 0;
}

#endif // BACKUP_LOG
#endif // TEST_LOGGER
