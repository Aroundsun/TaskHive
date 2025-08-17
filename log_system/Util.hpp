#pragma once
#include <sys/stat.h>
#include <sys/types.h>
#include <jsoncpp/json/json.h>
#include <filesystem>
#include <ctime>
#include <fstream>
#include <iostream>
#include <filesystem>
namespace logsystem
{
    class Data
    {
    public:
        // 获取当前时间戳
        static time_t GetCurrentTime()
        {
            time_t now = time(0);
            // tm *ltm = localtime(&now);
            // char buffer[80];
            // strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
            return now;
        }
    };

    class File
    {
    public:
        // 检查文件是否存在
        static bool FileExists(const std::string &filename)
        {
            // 使用 stat 函数检查文件是否存在
            // 如果文件存在，stat 函数返回 0，否则返回 -1
            struct stat buffer;
            return (stat(filename.c_str(), &buffer) == 0);
        }
        // 获取文件所在目录
        static std::string Path(const std::string &filename)
        {
            if (filename.empty())
                return "";
            int pos = filename.find_last_of("/\\"); // 查找最后一个斜杠或反斜杠的位置
            // 如果找到了斜杠或反斜杠，返回该位置之前的子字符串
            if (pos != std::string::npos)
                return filename.substr(0, pos + 1);
            return "";
        }
        // 创建目录
        static void CreateDirectory(const std::string &pathname)
        {
            if (pathname.empty())
            {
                throw std::invalid_argument("路径为空");
                return;
            }
            std::filesystem::path p(pathname);
            // 如果路径以 / 或 \ 结尾，说明是目录路径，直接处理
            //  否则取 parent_path（即文件所在目录
            std::filesystem::path dir = (std::filesystem::is_directory(p) || pathname.back() == '/' || pathname.back() == '\\') ? p : p.parent_path();


            if (!dir.empty() && !std::filesystem::exists(dir))
            {
                std::filesystem::create_directories(dir);
            }
        }

        // 获取文件大小
        int64_t GetFileSize(const std::string filename)
        {
            struct stat fileStat;
            auto result = stat(filename.c_str(), &fileStat);
            if (result != 0)
            {
                std::cerr << "GetFileSize: 无法获取文件大小，文件可能不存在: " << filename << std::endl;
                return -1; // 返回 -1 表示获取文件大小失败
            }
            return fileStat.st_size; // 返回文件大小
        }

        // 获取文件内容
        bool GetFileContent(std::string *content, std::string filename)
        {
            std::ifstream ifs;
            // 以二进制方式打开
            ifs.open(filename.c_str(), std::ios::in | std::ios::binary);
            // 文件打开失败
            if (!ifs)
            {
                std::cout << "GetFileContent: 无法打开文件: " << filename << std::endl;
                return false;
            }
            // 更改文件偏移指针到文件头p
            ifs.seekg(0, std::ios::beg);
            // 获取文件大小
            int64_t fileSize = GetFileSize(filename);
            if (fileSize < 0)
            {
                std::cout << "GetFileContent: 无法获取文件大小: " << filename << std::endl;
                return false;
            }
            content->resize(fileSize);
            // 读取文件内容到content中
            ifs.read(&(*content)[0], fileSize);
            if (!ifs.good())
            {
                std::cout << "GetFileContent: 读取文件内容失败: " << filename << std::endl;
                ifs.close();
                return false;
            }
            ifs.close();
            return true;
        }
    };

    // JSON 相关操作
    class JsonUtil
    {
    public:
        // 序列化 json
        static bool Serialize(const Json::Value &val, std::string* str)
        {
            // 建造者生成->建造者实例化json写对象->调用写对象中的接口进行序列化写入str
            Json::StreamWriterBuilder swb;
            std::unique_ptr<Json::StreamWriter> usw(swb.newStreamWriter());
            std::stringstream ss;
            if (usw->write(val, &ss) != 0)
            {
                std::cout << "serialize error" << std::endl;
                return false;
            }
            *str = ss.str();
            return true;
        }
        // 反序列化 json
        static bool DeSerialize(const std::string &str, Json::Value *val)
        {
            // 操作方法类似序列化
            Json::CharReaderBuilder crb;
            std::unique_ptr<Json::CharReader> ucr(crb.newCharReader()); //
            std::string err;
            if (ucr->parse(str.c_str(), str.c_str() + str.size(), val, &err) == false)
            {
                std::cout << __FILE__ << __LINE__ << "parse error" << err << std::endl;
                return false;
            }
            return true;
        }
    };

    // 配置类 读取配置文件获取日志系统的配置信息
    // 单例模式 懒汉
    class Config
    {
    public:
        static Config *GetInstance()
        {
            static Config instance; 
            return &instance;
        }

    private:
        Config()
        {
            std::string content;
            logsystem::File file;
            if (file.GetFileContent(&content, "./config.conf") == false)
            {
                std::cout << __FILE__ << __LINE__ << "open config.conf failed" << std::endl;
                perror(NULL);
            }
            Json::Value root;
            logsystem::JsonUtil::DeSerialize(content, &root); // 反序列化，把内容转成jaon value格式
            buffer_size = root["buffer_size"].asInt64();
            threshold = root["threshold"].asInt64();
            linear_growth = root["linear_growth"].asInt64();
            flush_log = root["flush_log"].asInt64();
            backup_addr = root["backup_addr"].asString();
            backup_port = root["backup_port"].asInt();
            thread_count = root["thread_count"].asInt();
        }

    public:
        size_t buffer_size;      // 缓冲区基础容量
        size_t threshold;        // 阈值容量
        size_t linear_growth;    // 线性增长容量
        size_t flush_log;        // 控制日志同步到磁盘的时机，默认为0,1调用fflush，2调用fsync
        std::string backup_addr; // 备份服务器地址
        uint16_t backup_port;    // 备份服务器端口
        size_t thread_count;     // 线程池线程数量
    };

} // namespace logsystem