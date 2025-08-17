#pragma once
#include <cassert>
#include <fstream>
#include <memory>
#include <unistd.h>
#include <fcntl.h>  // open
#include <unistd.h> // fsync, close

#include "Util.hpp"

extern logsystem::Config *config;

namespace logsystem
{
    // 日志刷新接口类
    class LogFlush
    {
    public:
        using ptr = std::shared_ptr<LogFlush>;
        virtual ~LogFlush() {};
        virtual void Flush(const char *data, size_t len) = 0;
    };

    // 标准输出日志刷新类
    class StdoutFlush : public LogFlush
    {
    public:
        using ptr = std::shared_ptr<StdoutFlush>;
        void Flush(const char *data, size_t len) override
        {
            assert(data != nullptr && len > 0);
            std::cout.write(data, len); // 将数据写入标准输出
            std::cout.flush();          // 刷新输出流
            if (config->flush_log == 1) // 如果配置要求使用 fflush 刷新
            {
                fflush(stdout); // 把用户缓冲写入操作系统缓存
            }
            else if (config->flush_log == 2) // 如果配置要求使用 fsync 刷新
            {
                int fd = fileno(stdout); // 获取标准输出的文件描述符
                if (fd >= 0)
                {
                    fflush(stdout); // 把用户缓冲写入操作系统缓存
                    fsync(fd);      // 刷新文件描述符到磁盘
                }
                else
                {
                    std::cerr << "StdoutFlush: 获取标准输出文件描述符失败" << std::endl;
                }
            }
            else
            {
                std::cerr << "StdoutFlush: 无效的刷新配置" << std::endl;
            }
        }
    };
    // 文件日志刷新类
    class FileFlush : public LogFlush
    {
    public:
        using ptr = std::shared_ptr<FileFlush>;
        FileFlush(const std::string filename)
            : filename_(filename)
        {
            try
            {
                // 创建所给目录
                logsystem::File::CreateDirectory(filename);
                
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "空目录：" << e.what() << std::endl;
            }
            ofs_.open(filename, std::ios::app | std::ios::binary);
            if (!ofs_)
            {
                std::cout << __FILE__ << __LINE__ << "open log file failed" << std::endl;
                perror(NULL);
            }
        }
        void Flush(const char *data, size_t len) override
        {
            // 写入
            ofs_.write(data, static_cast<std::streamsize>(len));
            // 检查写错误
            if (!ofs_)
            {
                std::cerr << __FILE__ << ":" << __LINE__ << "  write log file failed" << std::endl;
                throw std::runtime_error("ofstream write fail");
            }
            if (config->flush_log == 1)
            {
                ofs_.flush(); // 刷新输出流
            }
            else if (config->flush_log == 2)
            {
                ofs_.flush();  // 用户缓冲 -> 内核
                int fd = ::open(filename_.c_str(), O_WRONLY); //  重新拿 fd
                if (fd >= 0)
                {
                    ::fsync(fd); // 内核 -> 硬盘
                    ::close(fd);
                }
            }
            else
            {
                perror("open for fsync failed");
            }
        }

    private:
        std::ofstream ofs_;
        std::string filename_;
    };

    // 滚动文件日志刷新类
    class RollingFileFlush : public LogFlush
    {
    public:
        using ptr = std::shared_ptr<RollingFileFlush>;
        RollingFileFlush(std::string basename, size_t max_size)
            : base_name_(basename), max_size_(max_size), current_size_(0), cnt_(0), filename_(basename)
        {
            std::string path = logsystem::File::Path(basename); // 获取日志文件所在的目录
            if (!path.empty())
            {
                logsystem::File::CreateDirectory(path); // 创建目录
            }
        }
        void Flush(const char *data, size_t len) override
        {
            // 确保日志文件大小不满足滚动要求
            std::cout<< "RollingFileFlush::Flush"<< std::endl;
            InitLogFile();                                       
            ofs_.write(data, static_cast<std::streamsize>(len)); // 将数据写入到缓冲区 用户态
            if (!ofs_)
            {
                std::cerr << __FILE__ << ":" << __LINE__ << "  write log file failed" << std::endl;
                std::cout<< "RollingFileFlush::Flush: write log file failed" << std::endl;
                throw std::runtime_error("ofstream write fail");
            }
            current_size_ += len; // 更新当前文件大小
            if (config->flush_log == 1)
            {
                ofs_.flush(); // 刷新，将用户缓冲区的数据写入到内核缓冲区
            }
            else if (config->flush_log == 2)
            {
                ofs_.flush();                                 // 刷新，将用户缓冲区的数据写入到内核缓冲区
                int fd = ::open(filename_.c_str(), O_WRONLY); //  重新拿 fd
                if (fd >= 0)
                {
                    ::fsync(fd); // 内核 → 硬盘
                    ::close(fd);
                }
            }
            if (current_size_ >= max_size_) // 写完再检查是否滚动
                InitLogFile();
        }

    private:
        // 初始化日志文件
        void InitLogFile()
        {
            if (!ofs_.is_open() || current_size_ >= max_size_)
            {
                if (ofs_)
                {
                    ofs_.close();
                    
                }
                std::string new_filename = CreatLogFileName(); // 创建新的日志文件名

                if (new_filename == "")
                {
                    std::cerr << __FILE__ << ":" << __LINE__ << "  create log file name failed" << std::endl;
                    throw std::runtime_error("create log file name fail");
                }
                ofs_.open(new_filename, std::ios::app | std::ios::binary);
                if (!ofs_)
                {
                    std::cerr << __FILE__ << ":" << __LINE__ << "  open log file failed" << std::endl;
                    perror(NULL);
                    throw std::runtime_error("ofstream open fail");
                }
                // 重新统计文件大小（已存在则续写）
                current_size_ = std::filesystem::exists(filename_)
                                    ? std::filesystem::file_size(filename_)
                                    : 0;
            }
        }
        // 创建日志文件名
        std::string CreatLogFileName()
        {
            time_t now = logsystem::Data::GetCurrentTime();
            struct tm t;
            localtime_r(&now, &t); // 将当前时间转换为本地时间
            std::string filename = base_name_;
            filename += std::to_string(t.tm_year + 1900);
            filename += std::to_string(t.tm_mon + 1);
            filename += std::to_string(t.tm_mday);
            filename += std::to_string(t.tm_hour + 1);
            filename += std::to_string(t.tm_min + 1);
            filename += std::to_string(t.tm_sec + 1) + '-' +
                        std::to_string(cnt_++) + ".log";
            filename_ = filename; // 更新当前日志文件名


            return filename;
        }

    private:
        size_t current_size_;   // 当前文件大小
        size_t max_size_;       // 最大文件大小
        size_t cnt_;            // 记录滚动次数
        std::ofstream ofs_;     // 输出文件流
        std::string base_name_; // 基础文件名
        std::string filename_;  // 当前日志文件名
    };

    // 工厂类用于创建不同类型的日志刷新器
    class LogFlushFactory
    {
    public:
        using ptr = std::shared_ptr<LogFlushFactory>;
        LogFlushFactory() = default;

        template <typename FlushType, typename... Args>
        static std::shared_ptr<LogFlush> CreateLog(Args &&...args)
        {
            return std::make_shared<FlushType>(std::forward<Args>(args)...);
        }

    };
} // namespace logsystem
