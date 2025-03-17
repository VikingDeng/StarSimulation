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
private:
  double ra;
  double dec;
  double fov_w;
  double fov_h;
  double gamma;
  double exposure;
  std::string redis_host;
  int redis_port;

  // Helper function to check if a star is within the field of view
  bool isStarInFOV(double star_ra, double star_dec) const;

  // Helper function to connect to Redis
  redisContext* connectRedis() const;

public:
  observer(double initial_ra, double initial_dec, double initial_fov_w, double initial_fov_h,
           double initial_gamma = 0.0, double initial_exposure = 0.0,
           const std::string& redis_host_addr = "127.0.0.1", int redis_port_num = 6379);

  std::vector<star> FileterStarInView();

  // Getter and setter methods for observer parameters if needed
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
};

#endif //OBSERVER_H