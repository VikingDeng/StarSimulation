//
// Created by viking on 2025/3/14.
//

#include "observer.h"
#include <cmath>
#include <hiredis/hiredis.h>
#include <iostream>
#include <stdexcept>
#include <cstddef>

bool observer::isStarInFOV(double star_ra, double star_dec) const {
    // Assuming a simple rectangular FOV for now.
    // You might need a more sophisticated check depending on your projection.

    // Normalize RA difference considering wrap-around at 0/360 degrees
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
    if (redis_conn == nullptr) {
        return visible_stars; // Return empty vector if connection fails
    }

    redisReply* keys_reply = static_cast<redisReply*>(redisCommand(redis_conn, "KEYS *"));
    if (keys_reply == nullptr) {
        std::cerr << "Error executing Redis KEYS command: " << redis_conn->errstr << std::endl;
        freeReplyObject(keys_reply);
        redisFree(redis_conn);
        return visible_stars;
    }

    if (keys_reply->type == REDIS_REPLY_ARRAY) {
        std::cout << "开始筛选视野内的星星，总共找到 " << keys_reply->elements << " 个潜在目标..." << std::endl;
        for (std::size_t i = 0; i < keys_reply->elements; ++i) {
            redisReply* key_reply = keys_reply->element[i];
            if (key_reply->type == REDIS_REPLY_STRING) {
                std::string star_id = key_reply->str;

                // 每处理一定数量的星星，打印一下进度
                if ((i + 1) % 100 == 0) {
                    std::cout << "已处理 " << (i + 1) << " 个星星，当前处理 ID: " << star_id << std::endl;
                }

                redisReply* star_data_reply = static_cast<redisReply*>(redisCommand(redis_conn, "HGETALL %s", star_id.c_str()));

                if (star_data_reply == nullptr) {
                    std::cerr << "Error executing Redis HGETALL command for key: " << star_id << ": " << redis_conn->errstr << std::endl;
                } else if (star_data_reply->type == REDIS_REPLY_ARRAY && star_data_reply->elements >= 6) {
                    star current_star;
                    bool ra_found = false;
                    bool dec_found = false;
                    bool mag_found = false;

                    for (std::size_t j = 0; j < star_data_reply->elements; j += 2) {
                        std::string field = star_data_reply->element[j]->str;
                        std::string value = star_data_reply->element[j + 1]->str;

                        if (field == "ra") {
                            current_star.ra = std::stod(value);
                            ra_found = true;
                        } else if (field == "dec") {
                            current_star.dec = std::stod(value);
                            dec_found = true;
                        } else if (field == "magnitude") {
                            current_star.magnitude = std::stod(value);
                            mag_found = true;
                        }
                    }

                    if (ra_found && dec_found && mag_found && isStarInFOV(current_star.ra, current_star.dec)) {
                        visible_stars.push_back(current_star);
                    }
                }
                freeReplyObject(star_data_reply);
            }
        }
        std::cout << "星星筛选完成，共找到 " << visible_stars.size() << " 颗视野内的星星。" << std::endl;
    } else {
        std::cerr << "Error: Redis KEYS command did not return an array." << std::endl;
    }

    freeReplyObject(keys_reply);
    redisFree(redis_conn);
    return visible_stars;
}

//std::vector<star> observer::FileterStarInView() {
//    std::vector<star> visible_stars;
//    redisContext* redis_conn = connectRedis();
//    if (redis_conn == nullptr) {
//        return visible_stars; // Return empty vector if connection fails
//    }
//
//    redisReply* keys_reply = static_cast<redisReply*>(redisCommand(redis_conn, "KEYS *"));
//    if (keys_reply == nullptr) {
//        std::cerr << "Error executing Redis KEYS command: " << redis_conn->errstr << std::endl;
//        freeReplyObject(keys_reply);
//        redisFree(redis_conn);
//        return visible_stars;
//    }
//
//    if (keys_reply->type == REDIS_REPLY_ARRAY) {
//        for (std::size_t i = 0; i < keys_reply->elements; ++i) {
//            redisReply* key_reply = keys_reply->element[i];
//            if (key_reply->type == REDIS_REPLY_STRING) {
//                std::string star_id = key_reply->str;
//                redisReply* star_data_reply = static_cast<redisReply*>(redisCommand(redis_conn, "HGETALL %s", star_id.c_str()));
//
//                if (star_data_reply == nullptr) {
//                    std::cerr << "Error executing Redis HGETALL command for key: " << star_id << ": " << redis_conn->errstr << std::endl;
//                } else if (star_data_reply->type == REDIS_REPLY_ARRAY && star_data_reply->elements >= 6) {
//                    star current_star;
//                    bool ra_found = false;
//                    bool dec_found = false;
//                    bool mag_found = false;
//
//                    for (std::size_t j = 0; j < star_data_reply->elements; j += 2) {
//                        std::string field = star_data_reply->element[j]->str;
//                        std::string value = star_data_reply->element[j + 1]->str;
//
//                        if (field == "ra") {
//                            current_star.ra = std::stod(value);
//                            ra_found = true;
//                        } else if (field == "dec") {
//                            current_star.dec = std::stod(value);
//                            dec_found = true;
//                        } else if (field == "magnitude") {
//                            current_star.magnitude = std::stod(value);
//                            mag_found = true;
//                        }
//                    }
//
//                    if (ra_found && dec_found && mag_found && isStarInFOV(current_star.ra, current_star.dec)) {
//                        visible_stars.push_back(current_star);
//                    }
//                }
//                freeReplyObject(star_data_reply);
//            }
//        }
//    } else {
//        std::cerr << "Error: Redis KEYS command did not return an array." << std::endl;
//    }
//
//    freeReplyObject(keys_reply);
//    redisFree(redis_conn);
//    return visible_stars;
//}

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