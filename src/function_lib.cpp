#include <iostream>
#include <string>
#include <map>
#include <json/json.h>
#include <cstring>
#include <fstream> // Added for file operations
#include <algorithm> // Added for string manipulation
#include <cctype> // Added for character case conversion
#include <array> // Added for popen buffer

// 导出函数声明
extern "C" {
    // 字符串处理函数
    const char* reverse_string(const char* input);
    
    // 数学计算函数
    const char* add_numbers(const char* input);
    const char* multiply_numbers(const char* input);
    
    // 文件操作函数
    const char* read_file_content(const char* input);
    const char* write_file_content(const char* input);
    
    // 系统信息函数
    const char* get_system_info(const char* input);
    
    // 数据处理函数
    const char* process_data(const char* input);
    const char* filter_data(const char* input);
    
    // 网络相关函数
    const char* ping_host(const char* input);
    const char* check_port(const char* input);
    
}

// 辅助函数：解析JSON参数
std::map<std::string, std::string> parse_json_params(const char* input) {
    std::map<std::string, std::string> params;
    try {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(input, root)) {
            for (const auto& key : root.getMemberNames()) {
                params[key] = root[key].asString();
            }
        }
    } catch (...) {
        // 解析失败时返回空map
        return {};
    }
    return params;
}

// 辅助函数：创建JSON响应
std::string create_json_response(bool success, const std::string& message, const std::string& data = "") {
    Json::Value root;
    root["success"] = success;
    root["message"] = message;
    if (!data.empty()) {
        root["data"] = data;
    }
    
    Json::StreamWriterBuilder writer;
    return Json::writeString(writer, root);
}

// 字符串反转函数
const char* reverse_string(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        std::string text = params["text"];
        
        std::string reversed = text;
        std::reverse(reversed.begin(), reversed.end());
        
        result = create_json_response(true, "字符串反转成功", reversed);
    } catch (...) {
        result = create_json_response(false, "字符串反转失败");
    }
    
    return result.c_str();
}

// 数字相加函数
const char* add_numbers(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        double a = std::stod(params["a"]);
        double b = std::stod(params["b"]);
        
        double sum = a + b;
        result = create_json_response(true, "数字相加成功", std::to_string(sum));
    } catch (...) {
        result = create_json_response(false, "数字相加失败");
    }
    
    return result.c_str();
}

// 数字相乘函数
const char* multiply_numbers(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        double a = std::stod(params["a"]);
        double b = std::stod(params["b"]);
        
        double product = a * b;
        result = create_json_response(true, "数字相乘成功", std::to_string(product));
    } catch (...) {
        result = create_json_response(false, "数字相乘失败");
    }
    
    return result.c_str();
}

// 读取文件内容函数
const char* read_file_content(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        std::string filename = params["filename"];
        
        std::ifstream file(filename);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            result = create_json_response(true, "文件读取成功", content);
        } else {
            result = create_json_response(false, "无法打开文件: " + filename);
        }
    } catch (...) {
        result = create_json_response(false, "文件读取失败");
    }
    
    return result.c_str();
}

// 写入文件内容函数
const char* write_file_content(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        std::string filename = params["filename"];
        std::string content = params["content"];
        
        std::ofstream file(filename);
        if (file.is_open()) {
            file << content;
            file.close();
            result = create_json_response(true, "文件写入成功");
        } else {
            result = create_json_response(false, "无法写入文件: " + filename);
        }
    } catch (...) {
        result = create_json_response(false, "文件写入失败");
    }
    
    return result.c_str();
}

// 获取系统信息函数
const char* get_system_info(const char* input) {
    static std::string result;
    try {
        Json::Value info;
        info["platform"] = "Linux";
        info["compiler"] = "GCC";
        info["cxx_standard"] = "C++17";
        
        Json::StreamWriterBuilder writer;
        std::string info_str = Json::writeString(writer, info);
        result = create_json_response(true, "系统信息获取成功", info_str);
    } catch (...) {
        result = create_json_response(false, "系统信息获取失败");
    }
    
    return result.c_str();
}

// 数据处理函数
const char* process_data(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        std::string data = params["data"];
        std::string operation = params["operation"];
        
        std::string processed_data;
        if (operation == "uppercase") {
            std::transform(data.begin(), data.end(), std::back_inserter(processed_data), ::toupper);
        } else if (operation == "lowercase") {
            std::transform(data.begin(), data.end(), std::back_inserter(processed_data), ::tolower);
        } else if (operation == "trim") {
            processed_data = data;
            processed_data.erase(0, processed_data.find_first_not_of(" \t\n\r"));
            processed_data.erase(processed_data.find_last_not_of(" \t\n\r") + 1);
        } else {
            processed_data = data; // 默认不处理
        }
        
        result = create_json_response(true, "数据处理成功", processed_data);
    } catch (...) {
        result = create_json_response(false, "数据处理失败");
    }
    
    return result.c_str();
}

// 数据过滤函数
const char* filter_data(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        std::string data = params["data"];
        std::string filter_type = params["filter_type"];
        std::string filter_value = params["filter_value"];
        
        std::string filtered_data;
        if (filter_type == "contains") {
            if (data.find(filter_value) != std::string::npos) {
                filtered_data = data;
            }
        } else if (filter_type == "starts_with") {
            if (data.substr(0, filter_value.length()) == filter_value) {
                filtered_data = data;
            }
        } else if (filter_type == "ends_with") {
            if (data.length() >= filter_value.length() && 
                data.substr(data.length() - filter_value.length()) == filter_value) {
                filtered_data = data;
            }
        }
        
        if (!filtered_data.empty()) {
            result = create_json_response(true, "数据过滤成功", filtered_data);
        } else {
            result = create_json_response(false, "数据不匹配过滤条件");
        }
    } catch (...) {
        result = create_json_response(false, "数据过滤失败");
    }
    
    return result.c_str();
}

// Ping主机函数
const char* ping_host(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        std::string host = params["host"];
        
        std::string command = "ping -c 1 " + host;
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe) {
            std::array<char, 128> buffer;
            std::string output;
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                output += buffer.data();
            }
            pclose(pipe);
            
            if (output.find("1 packets transmitted, 1 received") != std::string::npos) {
                result = create_json_response(true, "Ping成功", output);
            } else {
                result = create_json_response(false, "Ping失败", output);
            }
        } else {
            result = create_json_response(false, "无法执行ping命令");
        }
    } catch (...) {
        result = create_json_response(false, "Ping操作失败");
    }
    
    return result.c_str();
}

// 检查端口函数
const char* check_port(const char* input) {
    static std::string result;
    try {
        auto params = parse_json_params(input);
        std::string host = params["host"];
        std::string port = params["port"];
        
        std::string command = "nc -z " + host + " " + port;
        int ret = system(command.c_str());
        
        if (ret == 0) {
            result = create_json_response(true, "端口检查成功", "端口 " + port + " 在 " + host + " 上开放");
        } else {
            result = create_json_response(false, "端口检查失败", "端口 " + port + " 在 " + host + " 上关闭");
        }
    } catch (...) {
        result = create_json_response(false, "端口检查操作失败");
    }
    
    return result.c_str();
} 
//给一个地址发送一封邮件 
