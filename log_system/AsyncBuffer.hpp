#pragma once
#include <vector>
#include "Util.hpp"
#include <assert.h>

extern logsystem::Config *config;

namespace logsystem
{
    // 日志缓冲类
    class Buffer
    {
    public:
        Buffer() : write_pos_(0), read_pos_(0)
        {
            buffer_.resize(config->buffer_size); // 初始化缓冲区
        }
        // 写入日志到缓冲区
        void Push(const char *data, size_t size)
        {
            TobeEnough(size);
            if (write_pos_ + size > buffer_.capacity()) //确保空间足够
            {
                std::cerr << "Push: 缓冲区空间不足，无法写入数据" << std::endl;
            }
            // 若写入超出了当前 size，需要先扩展 size()，否则 std::copy 会越界
            if (write_pos_ + size > buffer_.size())
            {
                buffer_.resize(write_pos_ + size); // 扩展缓冲区大小
            }
            std::copy(data,data+size, buffer_.begin() + write_pos_); // 将数据写入缓冲区
            write_pos_ += size; // 更新写入位置
        }
        bool isEmpty() const
        {
            return write_pos_ == read_pos_;
        }

        // 交换缓冲区
        void Swap(Buffer &other)
        {
            buffer_.swap(other.buffer_);
            std::swap(write_pos_, other.write_pos_);
            std::swap(read_pos_, other.read_pos_);
        }
        // 写空间剩余容量
        size_t WriteableSize() const
        {
            return buffer_.capacity() - write_pos_;
        }
        // 读空间剩余容量
        size_t ReadableSize() const
        {
            return write_pos_ - read_pos_;
        }

        // 重置缓冲区
        void reset()
        {
            write_pos_ = 0;
            read_pos_ = 0;
            // buffer_.clear();
            // buffer_.resize(buffer_size); // 重新初始化缓冲区
        }
        void MoveWritePos(int len)
        {
            assert(len <= WriteableSize());
            write_pos_ += len;
        }
        void MoveReadPos(int len)
        {
            assert(len <= ReadableSize());
            read_pos_ += len;
        }
        const char *Begin() { return &buffer_[read_pos_]; }
        char *ReadBegin(int len)// 获取当前读位置的指针
        {
            assert(len <= ReadableSize());
            return &buffer_[read_pos_];
        }
       
    private:
        // 检查缓冲区空间是否足够，不够则扩容
        void TobeEnough(size_t len)
        {
            // 缓冲区现有容量
            size_t cap = buffer_.capacity();
            //需要的容量 
            size_t need_cap = len + write_pos_;
            if(cap == 0)
            {
                cap = config->buffer_size; // 如果缓冲区容量为0，则设置为初始大小
            }
            // 如果现有容量小于需要的容量
            while (cap < need_cap)
            {
                if (cap < config->threshold)
                {
                    cap *=2;
                }
                else
                {
                    cap += config->linear_growth;
                }
                
            }
            // 扩容缓冲区
            buffer_.reserve(cap);
        }

    private:
        std::vector<char> buffer_; // 日志缓冲区
        size_t write_pos_;         // 写入位置
        size_t read_pos_;          // 读写位置
    };
} // namespace logsystem
