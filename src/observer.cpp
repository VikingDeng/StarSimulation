#include "observer.h"
#include <cmath>
#include <hiredis/hiredis.h>
#include <iostream>
#include <stdexcept>
#include <cstddef>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm> // For std::move
#include <atomic>   // For std::atomic

std::atomic<std::size_t> g_processed_star_count = 0; // 全局原子计数器

bool observer::isStarInFOV(double star_ra, double star_dec) const {
    double delta_ra = std::fmod(star_ra - ra + 360.0, 360.0);
    if (delta_ra > 180.0) {
        delta_ra -= 360.0;
    }
    double delta_dec = star_dec - dec;
    return std::abs(delta_ra) <= fov_w / 2.0 && std::abs(delta_dec) <= fov_h / 2.0;
}

redisContext* observer::connectRedis() const {
    redisContext* connection = redisConnect(redis_host.c_str(), redis_port);
    if (connection == nullptr || connection->err) {
        if (connection) {
            std::cerr << "Redis connection error: " << connection->errstr << std::endl;
            redisFree(connection);
        } else {
            std::cerr << "Can't allocate Redis context" << std::endl;
        }
        return nullptr;
    }
    return connection;
}

observer::observer(double initial_ra, double initial_dec, double initial_fov_w, double initial_fov_h,
                   double initial_gamma, double initial_exposure,
                   const std::string& redis_host_addr, int redis_port_num)
    : ra(initial_ra), dec(initial_dec), fov_w(initial_fov_w), fov_h(initial_fov_h),
      gamma(initial_gamma), exposure(initial_exposure), redis_host(redis_host_addr), redis_port(redis_port_num) {}

std::vector<star> observer::FileterStarInView() {
    std::vector<star> visible_stars;
    redisContext* redis_conn = connectRedis();
    if (redis_conn == nullptr) return visible_stars;
    redisReply* keys_reply = static_cast<redisReply*>(redisCommand(redis_conn, "KEYS *"));
    if (!keys_reply || keys_reply->type != REDIS_REPLY_ARRAY) { if (keys_reply) freeReplyObject(keys_reply); redisFree(redis_conn); return visible_stars; }
    std::cout << "开始筛选视野内的星星，总共找到 " << keys_reply->elements << " 个潜在目标..." << std::endl;
    for (std::size_t i = 0; i < keys_reply->elements; ++i) {
        redisReply* key_reply = keys_reply->element[i];
        if (key_reply->type == REDIS_REPLY_STRING) {
            std::string star_id = key_reply->str;
            if ((i + 1) % 10000 == 0) std::cout << "已处理 " << (i + 1) << " 个星星，当前处理 ID: " << star_id << std::endl;
            redisReply* star_data_reply = static_cast<redisReply*>(redisCommand(redis_conn, "HGETALL %s", star_id.c_str()));
            if (!star_data_reply) std::cerr << "Error executing Redis HGETALL command for key: " << star_id << ": " << redis_conn->errstr << std::endl;
            else if (star_data_reply->type == REDIS_REPLY_ARRAY && star_data_reply->elements >= 6) {
                star current_star; bool ra_found = false; bool dec_found = false; bool mag_found = false;
                for (std::size_t j = 0; j < star_data_reply->elements; j += 2) {
                    std::string field = star_data_reply->element[j]->str;
                    std::string value = star_data_reply->element[j + 1]->str;
                    if (field == "ra") { current_star.ra = std::stod(value); ra_found = true; }
                    else if (field == "dec") { current_star.dec = std::stod(value); dec_found = true; }
                    else if (field == "magnitude") { current_star.magnitude = std::stod(value); mag_found = true; }
                }
                if (ra_found && dec_found && mag_found && isStarInFOV(current_star.ra, current_star.dec)) {
                    visible_stars.push_back(current_star);
                }
            }
            freeReplyObject(star_data_reply);
        }
    }
    std::cout << "星星筛选完成，共找到 " << visible_stars.size() << " 颗视野内的星星。" << std::endl;
    freeReplyObject(keys_reply);
    redisFree(redis_conn);
    return visible_stars;
}

void process_star_keys_threaded(const std::vector<std::string>& keys, const observer& obs, std::vector<star>& local_visible_stars) {
    redisContext* redis_conn = obs.connectRedis();
    if (redis_conn == nullptr) {
        return;
    }

    size_t processed_count = 0;
    for (const auto& star_id : keys) {
        redisReply* star_data_reply = static_cast<redisReply*>(redisCommand(redis_conn, "HGETALL %s", star_id.c_str()));
        if (star_data_reply != nullptr && star_data_reply->type == REDIS_REPLY_ARRAY && star_data_reply->elements >= 6) {
            star current_star;
            bool ra_found = false;
            bool dec_found = false;
            bool mag_found = false;

            for (std::size_t j = 0; j < star_data_reply->elements; j += 2) {
                std::string field = star_data_reply->element[j]->str;
                std::string value = star_data_reply->element[j + 1]->str;
                if (field == "ra") {current_star.ra = std::stod(value);ra_found = true;}
                else if (field == "dec") {current_star.dec = std::stod(value);dec_found = true;}
                else if (field == "magnitude") {current_star.magnitude = std::stod(value);mag_found = true;}
            }

            if (ra_found && dec_found && mag_found && obs.isStarInFOV(current_star.ra, current_star.dec)) {
                local_visible_stars.push_back(current_star);
            }
        }
        freeReplyObject(star_data_reply);
        processed_count++;
        g_processed_star_count++; // 增加全局计数器
        if (processed_count % 10000 == 0) { // 每处理 1000 颗星星打印一次进度（可以调整）
            std::cout << "线程 " << std::this_thread::get_id() << " 已处理 " << processed_count << " 颗星星，总共处理 " << g_processed_star_count << " 颗..." << std::endl;
        }
    }
    redisFree(redis_conn);
}

std::vector<star> observer::FileterStarInViewMultithreaded(int num_threads) {
    std::vector<star> all_visible_stars;
    g_processed_star_count = 0; // 重置计数器

    redisContext* redis_conn = connectRedis();
    if (redis_conn == nullptr) {
        return all_visible_stars;
    }

    redisReply* keys_reply = static_cast<redisReply*>(redisCommand(redis_conn, "KEYS *"));
    if (keys_reply == nullptr || keys_reply->type != REDIS_REPLY_ARRAY) {
        if (keys_reply) freeReplyObject(keys_reply);
        redisFree(redis_conn);
        return all_visible_stars;
    }

    std::vector<std::string> all_keys;
    for (size_t i = 0; i < keys_reply->elements; ++i) {
        if (keys_reply->element[i]->type == REDIS_REPLY_STRING) {
            all_keys.push_back(keys_reply->element[i]->str);
        }
    }
    freeReplyObject(keys_reply);
    redisFree(redis_conn);

    if (all_keys.empty()) {
        return all_visible_stars;
    }

    std::cout << "开始多线程筛选视野内的星星，总共找到 " << all_keys.size() << " 个潜在目标..." << std::endl;

    int keys_per_thread = all_keys.size() / num_threads;
    int remaining_keys = all_keys.size() % num_threads;
    std::vector<std::thread> threads;
    std::vector<std::vector<star>> thread_results(num_threads);
    int start_index = 0;

    for (int i = 0; i < num_threads; ++i) {
        int end_index = start_index + keys_per_thread + (i < remaining_keys ? 1 : 0);
        std::vector<std::string> keys_chunk(all_keys.begin() + start_index, all_keys.begin() + end_index);
        threads.emplace_back(process_star_keys_threaded, keys_chunk, *this, std::ref(thread_results[i]));
        start_index = end_index;
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 合并所有线程的结果
    for (auto& result_list : thread_results) {
        all_visible_stars.insert(all_visible_stars.end(), result_list.begin(), result_list.end());
    }

    std::cout << "星星筛选完成，共找到 " << all_visible_stars.size() << " 颗视野内的星星 (多线程)." << std::endl;

    return all_visible_stars;
}

void observer::setRa(double new_ra) { ra = new_ra; }
double observer::getRa() const { return ra; }
void observer::setDec(double new_dec) { dec = new_dec; }
double observer::getDec() const { return dec; }
void observer::setFovW(double new_fov_w) { fov_w = new_fov_w; }
double observer::getFovW() const { return fov_w; }
void observer::setFovH(double new_fov_h) { fov_h = new_fov_h; }
double observer::getFovH() const { return fov_h; }
void observer::setGamma(double new_gamma) { gamma = new_gamma; }
double observer::getGamma() const { return gamma; }
void observer::setExposure(double new_exposure) { exposure = new_exposure; }
double observer::getExposure() const { return exposure; }