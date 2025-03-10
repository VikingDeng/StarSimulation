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

namespace {
    // 字段定义 (字节位置从0开始计算)
    struct FieldSpec {
        size_t start;
        size_t length;
        enum { INT, DOUBLE, CHAR } type;
        double scale;
    };

    const std::vector<std::pair<std::string, FieldSpec>> FIELD_DEFS = {
        // 核心定位与运动参数
        {"TYC1",    {0,   4, FieldSpec::INT,    1}},      // TYC标识符（可选，用于唯一标识）
        {"mRAdeg",  {15, 12, FieldSpec::DOUBLE, 1}},      // 平均赤经（ICRS J2000.0，单位：度）
        {"mDEdeg",  {28, 12, FieldSpec::DOUBLE, 1}},      // 平均赤纬（ICRS J2000.0，单位：度）
        {"pmRA",    {41,  7, FieldSpec::DOUBLE, 0.001}},  // 赤经自行（单位：角秒/年，原始数据为毫角秒/年）
        {"pmDE",    {49,  7, FieldSpec::DOUBLE, 0.001}},  // 赤纬自行（单位：角秒/年）
        {"mepRA",   {76,  7, FieldSpec::DOUBLE, 1}},      // 赤经平均历元（单位：年）
        {"mepDE",   {83,  7, FieldSpec::DOUBLE, 1}},      // 赤纬平均历元（单位：年）

        // 星等参数（必要）
        {"BT",      {111, 6, FieldSpec::DOUBLE, 1}},      // Tycho-2 B_T星等（单位：星等）
        {"VT",      {124, 6, FieldSpec::DOUBLE, 1}},      // Tycho-2 V_T星等（单位：星等）

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
}

void Tycho2Dataset::ProcessDirectory(const std::string& data_dir) {
    const std::string pattern = data_dir + "/tyc2.dat.*";

    try {
        std::vector<std::thread> workers;

        for(auto files = Glob(pattern); const auto& file: files) {
            workers.emplace_back([this, file] {
                ProcessFile(file);
            });
        }

        for(auto& t : workers) {
            t.join();
        }
    } catch(const std::exception& e) {
        std::lock_guard <std::mutex> lock(io_mutex_);
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Tycho2Dataset::ProcessFile(const std::string& file_path) {
    redisContext* c = redisConnect(redis_host_.c_str(), redis_port_);
    if(c == nullptr || c->err) {
        std::lock_guard <std::mutex> lock(io_mutex_);
        std::cerr << "Redis connection error: "
                 << (c ? c->errstr : "can't allocate context")
                 << std::endl;
        return;
    }

    std::ifstream fin(file_path);
    if(!fin) {
        std::lock_guard <std::mutex> lock(io_mutex_);
        std::cerr << "Failed to open: " << file_path << std::endl;
        redisFree(c);
        return;
    }

    std::vector<std::string> redis_cmds;
    redis_cmds.reserve(batch_size_);
    std::string line;

    while(std::getline(fin, line)) {
        try {
            Tycho2Entry entry = ParseLine(line);

            std::string cmd =
                "HSET tyc:" + entry.TYC_ID +
                " TYC1 " + std::to_string(entry.TYC1) +
                " TYC2 " + std::to_string(entry.TYC2) +
                " TYC3 " + std::to_string(entry.TYC3) +
                " pflag " + std::string(1, entry.pflag) +
                " RAmdeg " + std::to_string(entry.RAmdeg) +
                " DEpmdeg " + std::to_string(entry.DEpmdeg) +
                " pmRA " + std::to_string(entry.pmRA) +
                " pmDE " + std::to_string(entry.pmDE);

            redis_cmds.push_back(cmd);

            if(redis_cmds.size() >= batch_size_) {
                FlushRedisBatch(c, redis_cmds);
            }
        } catch(const std::exception& e) {
            std::lock_guard <std::mutex> lock(io_mutex_);
            std::cerr << "Parse error: " << e.what()
                     << "\nLine: " << line << std::endl;
        }
    }

    // 刷新剩余数据
    if(!redis_cmds.empty()) {
        FlushRedisBatch(c, redis_cmds);
    }

    redisFree(c);
}

Tycho2Entry Tycho2Dataset::ParseLine(const std::string& line) {
    Tycho2Entry entry;

    for(const auto& [name, spec] : FIELD_DEFS) {
        if(spec.start + spec.length > line.length()) {
            throw std::out_of_range("Field " + name + " out of line boundary");
        }

        std::string raw = line.substr(spec.start, spec.length);
        size_t pos = 0;

        try {
            if(spec.type == FieldSpec::INT) {
                int val = std::stoi(raw, &pos);
                if(name == "TYC1") entry.TYC1 = val;
                else if(name == "TYC2") entry.TYC2 = val;
                else if(name == "TYC3") entry.TYC3 = val;
                else if(name == "e_RAmdeg") entry.e_RAmdeg = val;
                else if(name == "e_DEpmdeg") entry.e_DEpmdeg = val;
            }
            else if(spec.type == FieldSpec::DOUBLE) {
                double val = std::stod(raw) * spec.scale;
                if(name == "RAmdeg") entry.RAmdeg = val;
                else if(name == "DEpmdeg") entry.DEpmdeg = val;
                else if(name == "pmRA") entry.pmRA = val;
                else if(name == "pmDE") entry.pmDE = val;
                else if(name == "e_pmRA") entry.e_pmRA = val;
                else if(name == "e_pmDE") entry.e_pmDE = val;
            }
            else if(spec.type == FieldSpec::CHAR) {
                if(name == "pflag") entry.pflag = raw.empty() ? ' ' : raw[0];
            }
        } catch(...) {
            throw std::runtime_error("Invalid " + name + " value: " + raw);
        }
    }

    entry.TYC_ID = std::to_string(entry.TYC1) + "-"
                 + std::to_string(entry.TYC2) + "-"
                 + std::to_string(entry.TYC3);

    return entry;
}

void Tycho2Dataset::FlushRedisBatch(redisContext* c,
                                  std::vector<std::string>& cmds) {
    for(const auto& cmd : cmds) {
        if(redisAppendCommand(c, cmd.c_str()) != REDIS_OK) {
            std::lock_guard <std::mutex> lock(io_mutex_);
            std::cerr << "Redis append error: " << c->errstr << std::endl;
            cmds.clear();
            return;
        }
    }

    for(size_t i=0; i<cmds.size(); ++i) {
        redisReply* reply;
        if(redisGetReply(c, (void**)&reply) != REDIS_OK) {
            std::lock_guard <std::mutex> lock(io_mutex_);
            std::cerr << "Redis error: " << c->errstr << std::endl;
            freeReplyObject(reply);
            break;
        }
        freeReplyObject(reply);
    }

    cmds.clear();
}