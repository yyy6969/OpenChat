#include "ConfigMgr.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

    // 默认配置文件名
    constexpr const char* kConfigFileName = "config.ini";
    // 可用于覆盖配置文件路径的环境变量名
    constexpr const char* kConfigEnvName = "GATESERVER_CONFIG";

    // 获取当前可执行文件自身的完整路径
    // Windows：GetModuleFileNameW（可能触发路径长度扩容）
    // Linux：  读取 /proc/self/exe 符号链接
    // 兜底：   返回当前工作目录
    std::filesystem::path ExecutablePath() {
#ifdef _WIN32
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;) {
            const DWORD size = GetModuleFileNameW(nullptr, buffer.data(),
                                                  static_cast<DWORD>(buffer.size()));
            if (size == 0) {
                break;                          // 调用失败，走下面的兜底逻辑
            }
            if (size < buffer.size()) {         // 成功且未截断
                buffer.resize(size);
                return std::filesystem::path(buffer);
            }
            buffer.resize(buffer.size() * 2);   // 缓冲区不够，扩容再试
        }
#elif defined(__linux__)
        std::error_code ec;
        const auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            return path;
        }
#endif

        // 兜底：拿不到 exe 路径就用当前工作目录
        std::error_code cwd_ec;
        const auto cwd = std::filesystem::current_path(cwd_ec);
        return cwd_ec ? std::filesystem::path() : cwd;
    }

    // 决定 config.ini 到底该从哪读
    // 优先级：环境变量 > 可执行文件所在目录 > 当前工作目录
    // 之所以优先 exe 目录：IDE 运行时 cwd 常常是工程根，
    // 而构建系统一般会把 config.ini 拷到 exe 旁边
    std::filesystem::path ResolveConfigPath() {
        // ① 环境变量显式指定
        if (const char* env_path = std::getenv(kConfigEnvName);
            env_path != nullptr && env_path[0] != '\0') {
            const std::filesystem::path path(env_path);
            if (!std::filesystem::exists(path)) {
                throw std::runtime_error(
                    std::string(kConfigEnvName) + " points to a missing file: " + path.string());
            }
            return path;
        }

        // 收集候选路径
        std::vector<std::filesystem::path> candidates;
        const auto exe_dir = ExecutablePath().parent_path();
        const auto cwd = std::filesystem::current_path();

        if (!exe_dir.empty()) {
            candidates.push_back(exe_dir / kConfigFileName);
        }
        if (cwd != exe_dir) {                   // 避免重复检查同一个目录
            candidates.push_back(cwd / kConfigFileName);
        }

        // 按顺序找，第一个存在的就用
        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) {
                return candidate;
            }
        }

        // 全都找不到 → 报错，并把查过的路径都打出来，方便排查
        std::ostringstream message;
        message << kConfigFileName << " not found. Searched:\n";
        for (const auto& candidate : candidates) {
            message << "  " << candidate << '\n';
        }
        message << "Set " << kConfigEnvName << " to override.";
        throw std::runtime_error(message.str());
    }

}  // namespace


// 构造函数：定位 config.ini → 解析 → 填充 _config_map
ConfigMgr::ConfigMgr() {
    const auto config_path = ResolveConfigPath();
    std::cout << "Config path: " << config_path << std::endl;

    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_ini(config_path.string(), pt);
    } catch (const boost::property_tree::ini_parser_error& e) {
        // 把 boost 的底层错误转成带文件名的清晰错误
        throw std::runtime_error("failed to parse " + config_path.string() + ": " + e.what());
    }

    // 遍历每个 [section]，把 key=value 装进 SectionInfo
    for (const auto& section_pair : pt) {
        SectionInfo section_info;
        for (const auto& key_value_pair : section_pair.second) {
            section_info._section_datas[key_value_pair.first] =
                key_value_pair.second.get_value<std::string>();
        }
        _config_map[section_pair.first] = std::move(section_info);
    }
}

// 单例入口：C++11 起 static 局部变量初始化是线程安全的
ConfigMgr& ConfigMgr::Inst() {
    static ConfigMgr instance;
    return instance;
}

// 软查找：找不到时返回静态空 SectionInfo 的引用
const SectionInfo& ConfigMgr::operator[](const std::string& section) const {
    static const SectionInfo empty;         // 进程级生命周期，返回引用安全
    auto it = _config_map.find(section);
    return it == _config_map.end() ? empty : it->second;
}

// 硬查找：找不到时抛 out_of_range
const SectionInfo& ConfigMgr::At(const std::string& section) const {
    auto it = _config_map.find(section);
    if (it == _config_map.end()) {
        throw std::out_of_range("config section not found: [" + section + "]");
    }
    return it->second;
}

// 判断是否存在该 section
bool ConfigMgr::Contains(const std::string& section) const {
    return _config_map.find(section) != _config_map.end();
}

// 便捷接口：一步拿值，等价于 (*this)[section][key]
const std::string& ConfigMgr::GetValue(const std::string& section,
                                       const std::string& key) const {
    return (*this)[section][key];
}

// 软查找：缺失 key 时返回静态空串的引用
const std::string& SectionInfo::operator[](const std::string& key) const {
    // 静态空字符串生命周期贯穿整个进程，返回引用是安全的
    static const std::string empty;
    auto it = _section_datas.find(key);
    return it == _section_datas.end() ? empty : it->second;
}

// 硬查找：缺失 key 时抛 out_of_range
const std::string& SectionInfo::At(const std::string& key) const {
    auto it = _section_datas.find(key);
    if (it == _section_datas.end()) {
        throw std::out_of_range("config key not found: [" + key + "]");
    }
    return it->second;
}

// 判断是否包含某个 key
bool SectionInfo::Contains(const std::string& key) const {
    return _section_datas.find(key) != _section_datas.end();
}