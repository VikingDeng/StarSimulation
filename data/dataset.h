//
// Created by viking on 2025/3/9.
//

#ifndef DATASET_H
#define DATASET_H

#include <vector>
#include <string>
#include <mutex>
#include <hiredis/hiredis.h>

struct Tycho2Entry {
    // 标识符
    int TYC1;
    int TYC2;
    int TYC3;
    std::string TYC_ID;

    // 天体测量参数
    double mRAdeg;    // 平均赤经（度）
    double mDEdeg;    // 平均赤纬（度）
    double pmRA;      // 赤经自行（角秒/年）
    double pmDE;      // 赤纬自行（角秒/年）
    double mepRA;     // 赤经平均历元（年）
    double mepDE;     // 赤纬平均历元（年）

    // 星等参数
    double BT;
    double VT;
    double V_mag;     // 计算后的可视星等
};

class Tycho2Dataset {
    public:
      explicit Tycho2Dataset(const std::string& redis_host = "127.0.0.1",
                             int redis_port = 6379,
                             size_t batch_size = 1000)
          : redis_host_(redis_host),
            redis_port_(redis_port),
            batch_size_(batch_size){}

      void ProcessDirectory(const std::string& data_dir);

    private:
      void ProcessFile(const std::string& file_path);
      Tycho2Entry ParseLine(const std::string& line);
      void FlushRedisBatch(redisContext* c, std::vector<std::string>& cmds);

      const std::string redis_host_;
      const int redis_port_;
      const size_t batch_size_;
      std::mutex io_mutex_;
};

#endif //DATASET_H
