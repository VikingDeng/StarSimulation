//
// Created by viking on 2025/3/9.
//

#include "dataset.h"
#include <glob.h>
#include <cstring>
#include <thread>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>
#include <atomic>
#include <cmath>

namespace {
    // 字段定义 (字节位置从0开始计算)
    struct FieldSpec {
        size_t start;
        size_t length;
        enum { INT, DOUBLE, CHAR } type;
        double scale;
    };

    const std::vector<std::pair<std::string, FieldSpec>> FIELD_DEFS = {
        {"TYC1",    {0,   4, FieldSpec::INT,    1}},
        {"TYC2",    {5,   5, FieldSpec::INT,    1}},     // 修正长度
        {"TYC3",    {11,  2, FieldSpec::INT,    1}},     // 修正位置
        {"mRAdeg",  {15, 13, FieldSpec::DOUBLE, 1}},
        {"mDEdeg",  {28, 13, FieldSpec::DOUBLE, 1}},
        {"pmRA",    {41,  8, FieldSpec::DOUBLE, 0.001}}, // 单位：mas/yr
        {"pmDE",    {49,  8, FieldSpec::DOUBLE, 0.001}}, // 单位：mas/yr
        {"mepRA",   {75,  8, FieldSpec::DOUBLE, 1}},
        {"mepDE",   {83,  8, FieldSpec::DOUBLE, 1}},
        {"BT",      {110, 7, FieldSpec::DOUBLE, 1}},
        {"VT",      {123, 7, FieldSpec::DOUBLE, 1}},
    };

    std::vector<std::string> Glob(const std::string& pattern) {
        glob_t glob_result = {};

        const int ret = glob(pattern.c_str(), GLOB_TILDE, nullptr, &glob_result);
        if(ret != 0) {
            globfree(&glob_result);
            throw std::runtime_error("glob failed: " + pattern);
        }

        std::vector<std::string> files;
        for(size_t i=0; i<glob_result.gl_pathc; ++i) {
            files.emplace_back(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
        return files;
    }

    void trim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    }

    // 进度条相关变量
    std::atomic<size_t> total_lines_processed(0); // 已处理的总行数
    std::atomic<size_t> total_lines_to_process(0); // 需要处理的总行数
    std::mutex progress_mutex;

    // 进度条更新函数
    void UpdateProgress() {
        static auto start_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
        size_t processed = total_lines_processed.load();
        size_t total = total_lines_to_process.load();

        if (total == 0) return;

        double progress = static_cast<double>(processed) / total;
        int bar_width = 50;

        std::lock_guard<std::mutex> lock(progress_mutex);
        std::cout << "[" << std::string(static_cast<int>(bar_width * progress), '#')
                  << std::string(bar_width - static_cast<int>(bar_width * progress), ' ')
                  << "] " << std::setprecision(2) << std::fixed << progress * 100 << "% ("
                  << processed << "/" << total << ") - "
                  << "Elapsed: " << elapsed << "s - "
                  << "ETA: " << static_cast<int>(elapsed / progress - elapsed) << "s\r";
        std::cout.flush();
    }

    // 统计文件总行数
    size_t CountLines(const std::string& file_path) {
        std::ifstream fin(file_path);
        if (!fin) return 0;

        size_t count = 0;
        std::string line;
        while (std::getline(fin, line)) {
            count++;
        }
        return count;
    }

    // 统计所有文件总行数
    void CountTotalLines(const std::vector<std::string>& files) {
        size_t total = 0;
        for (const auto& file : files) {
            total += CountLines(file);
        }
        total_lines_to_process.store(total);
    }

    // 记录数据库当前状态
    void LogDatabaseStatus(redisContext* c, const std::string& log_file) {
        std::ofstream fout(log_file, std::ios::app);
        if (!fout) {
            std::cerr << "Failed to open log file: " << log_file << std::endl;
            return;
        }

        fout << "Database status before insertion:" << std::endl;

        // 获取数据库信息
        if (redisAppendCommand(c, "INFO") != REDIS_OK) {
            std::cerr << "Redis append error: " << c->errstr << std::endl;
            return;
        }

        redisReply* reply;
        if (redisGetReply(c, (void**)&reply) != REDIS_OK) {
            std::cerr << "Redis error: " << c->errstr << std::endl;
            freeReplyObject(reply);
            return;
        }

        if (reply->type == REDIS_REPLY_STRING) {
            fout << reply->str << std::endl;
        } else {
            fout << "Failed to get database info" << std::endl;
        }

        freeReplyObject(reply);
        fout << "----------------------------------------" << std::endl;
    }

    // 获取当前年份作为临时观测时间
    int getCurrentYear() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_r(&now_c, &now_tm);
        return now_tm.tm_year + 1900;
    }
}

void Tycho2Dataset::ProcessDirectory(const std::string& data_dir) {
    const std::string pattern = data_dir + "/tyc2.dat.*";

    try {
        std::vector<std::thread> workers;
        auto files = Glob(pattern);

        // 连接到 Redis
        redisContext* c = redisConnect(redis_host_.c_str(), redis_port_);
        if (c == nullptr || c->err) {
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cerr << "Redis connection error: "
                      << (c ? c->errstr : "can't allocate context")
                      << std::endl;
            return;
        }

        // 记录数据库当前状态
        // LogDatabaseStatus(c, "database_status_before_insertion.log");

        // 统计总行数
        CountTotalLines(files);

        // 启动进度条更新线程
        std::thread progress_thread([&]() { // 使用引用捕获
            while (total_lines_processed.load() < total_lines_to_process.load()) {
                UpdateProgress();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            UpdateProgress(); // 最后一次更新
            std::cout << std::endl;
        });

        // 获取当前年份作为临时观测时间
        int currentYear = getCurrentYear();

        for (const auto& file : files) {
            workers.emplace_back([this, file, currentYear] {
                ProcessFile(file, currentYear);
            });
        }

        for (auto& t : workers) {
            t.join();
        }

        progress_thread.join();

        // 记录数据库插入后的状态
        LogDatabaseStatus(c, "database_status_after_insertion.log");

        redisFree(c);
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(io_mutex_);
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Tycho2Dataset::ProcessFile(const std::string& file_path, int currentYear) {
    redisContext* c = redisConnect(redis_host_.c_str(), redis_port_);
    if (c == nullptr || c->err) {
        std::lock_guard<std::mutex> lock(io_mutex_);
        std::cerr << "Redis connection error: "
                  << (c ? c->errstr : "can't allocate context")
                  << std::endl;
        return;
    }

    std::ifstream fin(file_path);
    if (!fin) {
        std::lock_guard<std::mutex> lock(io_mutex_);
        std::cerr << "Failed to open: " << file_path << std::endl;
        redisFree(c);
        return;
    }

    std::vector<std::string> redis_cmds;
    redis_cmds.reserve(batch_size_);
    std::string line;

    while (std::getline(fin, line)) {
        try {
            Tycho2Entry entry = ParseLine(line);

            // 1. 根据星历把星的赤经赤纬调整到现在的位置
            double deltaT_ra = currentYear - entry.mepRA;
            double deltaT_de = currentYear - entry.mepDE;

            // pmRA 和 pmDE 的单位是 milliarcseconds/year，需要转换为 degrees/year
            double pmRA_deg_per_year = entry.pmRA / 3600000.0;
            double pmDE_deg_per_year = entry.pmDE / 3600000.0;

            entry.mRAdeg += pmRA_deg_per_year * deltaT_ra;
            entry.mDEdeg += pmDE_deg_per_year * deltaT_de;

            // 将赤经归一化到 0-360 度
            while (entry.mRAdeg < 0) entry.mRAdeg += 360.0;
            while (entry.mRAdeg >= 360.0) entry.mRAdeg -= 360.0;

            // 2. 存到数据库里面的星只需要3个条目：赤经、赤纬、星等
            std::string cmd =
                "HSET " + entry.TYC_ID +
                " ra " + std::to_string(entry.mRAdeg) +
                " dec " + std::to_string(entry.mDEdeg) +
                " magnitude " + std::to_string(entry.V_mag);

            redis_cmds.push_back(cmd);

            if (redis_cmds.size() >= batch_size_) {
                FlushRedisBatch(c, redis_cmds);
            }

            // 更新已处理行数
            total_lines_processed.fetch_add(1, std::memory_order_relaxed);
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cerr << "Parse error: " << e.what()
                      << "\nLine: " << line << std::endl;
        }
    }

    // 刷新剩余数据
    if (!redis_cmds.empty()) {
        FlushRedisBatch(c, redis_cmds);
    }

    redisFree(c);
}

Tycho2Entry Tycho2Dataset::ParseLine(const std::string& line) {
    Tycho2Entry entry;

    for (const auto& [name, spec] : FIELD_DEFS) {
        if (spec.start + spec.length > line.length()) {
            throw std::out_of_range("Field " + name + " out of line boundary");
        }

        std::string raw = line.substr(spec.start, spec.length);
        trim(raw); // 需要实现trim函数去除空格

        try {
            if (spec.type == FieldSpec::INT) {
                int val = raw.empty() ? 0 : std::stoi(raw);
                if (name == "TYC1") entry.TYC1 = val;
                else if (name == "TYC2") entry.TYC2 = val;
                else if (name == "TYC3") entry.TYC3 = (val == 0) ? 1 : val; // 处理空值
            } else if (spec.type == FieldSpec::DOUBLE) {
                double val = raw.empty() ? 0.0 : std::stod(raw) * spec.scale;
                if (name == "mRAdeg") entry.mRAdeg = val;
                else if (name == "mDEdeg") entry.mDEdeg = val;
                else if (name == "pmRA") entry.pmRA = val;
                else if (name == "pmDE") entry.pmDE = val;
                else if (name == "mepRA") entry.mepRA = val;
                else if (name == "mepDE") entry.mepDE = val;
                else if (name == "BT") entry.BT = val;
                else if (name == "VT") entry.VT = val;
            }
        } catch (...) {
            throw std::runtime_error("Invalid " + name + " value: " + raw);
        }
    }

    // 计算可视星等
    if (entry.BT > 0 && entry.VT > 0) {
        double BV = entry.BT - entry.VT;
        entry.V_mag = entry.VT - 0.090 * BV;
    } else {
        entry.V_mag = 99.9; // 无效值标记
    }

    // 生成标准TYC标识
    entry.TYC_ID = std::to_string(entry.TYC1) + "-"
                 + std::to_string(entry.TYC2) + "-"
                 + std::to_string(entry.TYC3);

    return entry;
}

void Tycho2Dataset::FlushRedisBatch(redisContext* c,
                                  std::vector<std::string>& cmds) {
    for (const auto& cmd : cmds) {
        if (redisAppendCommand(c, cmd.c_str()) != REDIS_OK) {
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cerr << "Redis append error: " << c->errstr << std::endl;
            cmds.clear();
            return;
        }
    }

    for (size_t i = 0; i < cmds.size(); ++i) {
        redisReply* reply;
        if (redisGetReply(c, (void**)&reply) != REDIS_OK) {
            std::lock_guard<std::mutex> lock(io_mutex_);
            std::cerr << "Redis error: " << c->errstr << std::endl;
            freeReplyObject(reply);
            break;
        }
        freeReplyObject(reply);
    }

    cmds.clear();
}