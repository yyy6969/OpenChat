#pragma once

#include <map>
#include <stdexcept>
#include <string>

// 配置系统说明：
// 1. ConfigMgr 首次调用 Inst() 时加载 config.ini，之后配置只读，不再修改。
// 2. 因此 operator[] / At 返回 const 引用，避免每次访问都拷贝整个 SectionInfo。
// 3. operator[] 对缺失的 section/key 返回静态空对象；At 则抛出异常，便于尽早发现配置错误。
struct SectionInfo {
    SectionInfo() = default;
    SectionInfo(const SectionInfo&) = default;
    SectionInfo& operator=(const SectionInfo&) = default;

    // 缺失 key 时返回静态空字符串的引用，不产生拷贝。
    const std::string& operator[](const std::string& key) const;

    // 缺失 key 时抛出 std::out_of_range。
    const std::string& At(const std::string& key) const;

    bool Contains(const std::string& key) const;

private:
    friend class ConfigMgr;

    std::map<std::string, std::string> _section_datas;
};

class ConfigMgr {
public:
    // C++11 起静态局部变量初始化是线程安全的，多线程首次调用也安全。
    static ConfigMgr& Inst();

    // 缺失 section 时返回静态空 SectionInfo 的引用，不产生拷贝。
    const SectionInfo& operator[](const std::string& section) const;

    // 缺失 section 时抛出 std::out_of_range。
    const SectionInfo& At(const std::string& section) const;

    bool Contains(const std::string& section) const;

    // 便捷接口，等价于 (*this)[section][key]，同样返回引用。
    const std::string& GetValue(const std::string& section, const std::string& key) const;

    ConfigMgr(const ConfigMgr&) = delete;
    ConfigMgr& operator=(const ConfigMgr&) = delete;

private:
    ConfigMgr();

    std::map<std::string, SectionInfo> _config_map;
};
