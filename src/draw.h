//
// Created by viking on 2025/3/14.
//

#ifndef STARSIMULATION_DRAW_H
#define STARSIMULATION_DRAW_H

#include <vector>
#include <string>
#include <common.h>
#include <opencv2/core/mat.hpp>


namespace StarMapDrawer {
    void drawStarMap(const std::vector<star>& stars,
                 const std::string& outputPath,
                 int imageWidth, int imageHeight,
                 double centerRA, double centerDec, double fovRA, double fovDec,
                 double magnitudeThreshold = 12.0);

}

#endif