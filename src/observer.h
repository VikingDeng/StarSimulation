//
// Created by viking on 2025/3/14.
//

#ifndef OBSERVER_H
#define OBSERVER_H
#include <vector>
#include <string>
#include <common.h>
#include <hiredis/hiredis.h>

class observer {
public:
    observer(double initial_ra, double initial_dec, double initial_fov_w, double initial_fov_h,
         double initial_gamma = 0.0, double initial_exposure = 0.0,
         const std::string& redis_host_addr = "127.0.0.1", int redis_port_num = 6379);

    bool isStarInFOV(double star_ra, double star_dec) const;
    redisContext* connectRedis() const;
    std::vector<star> FileterStarInView(); // 原始的单线程版本
    std::vector<star> FileterStarInViewMultithreaded(int num_threads); // 多线程版本

    void setRa(double new_ra);
    double getRa() const;

    void setDec(double new_dec);
    double getDec() const;

    void setFovW(double new_fov_w);
    double getFovW() const;

    void setFovH(double new_fov_h);
    double getFovH() const;

    void setGamma(double new_gamma);
    double getGamma() const;

    void setExposure(double new_exposure);
    double getExposure() const;

private:
    double ra;
    double dec;
    double fov_w;
    double fov_h;
    double gamma;
    double exposure;
    std::string redis_host;
    int redis_port;
};

#endif //OBSERVER_H

