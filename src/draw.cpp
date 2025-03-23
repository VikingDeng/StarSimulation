//
// Created by viking on 2025/3/14.
//

#include "draw.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <random>
#include <vector>

namespace StarMapDrawer {

// --- 全局可调参数 ---
constexpr double MAGNITUDE_SCALE = 0.4;
constexpr double BASE_LUMINOSITY = 255.0; // 恢复到原来的值
constexpr double MIN_RADIUS = 0.5;
constexpr double BASE_MAX_RADIUS = 8.0; // 将原始 MAX_RADIUS 重命名为 BASE_MAX_RADIUS
constexpr double BASE_PSF_FWHM_SCALE = 2.5; // 存储基础的 PSF_FWHM_SCALE
constexpr double PSF_BETA = 2.0;        // 调整 PSF 的 beta 参数
constexpr double PSF_KERNEL_SIZE_MULTIPLIER = 3.0; // PSF 核大小的倍数
constexpr double SATURATION_MAGNITUDE = 6.0; // 饱和星等的阈值，比这个亮度高的星会发生饱和
constexpr int REFERENCE_RESOLUTION = 512; // 参考分辨率大小

// --- 随机数生成器 ---
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> jitter(-0.2, 0.2); // 用于位置抖动
std::uniform_real_distribution<> brightness_noise(-0.05, 0.05); // 用于亮度噪声

double intensityFromMagnitude(double magnitude) {
    return std::pow(10, -MAGNITUDE_SCALE * magnitude);
}

// 修改后的 drawStarMap 函数，添加 magnitudeThreshold 参数
void drawStarMap(const std::vector<star>& stars,
                 const std::string& outputPath,
                 int imageWidth, int imageHeight,
                 double centerRA, double centerDec, double fovRA, double fovDec,
                 double magnitudeThreshold ) { // 默认阈值为 12 等星
    cv::Mat accumulationBuffer(imageHeight, imageWidth, CV_32FC3, cv::Scalar(0, 0, 0));

    // 根据图像尺寸动态计算 MAX_RADIUS
    double resolutionScale = static_cast<double>(std::max(imageWidth, imageHeight)) / REFERENCE_RESOLUTION;
    double current_max_radius = BASE_MAX_RADIUS * resolutionScale;
    double current_psf_fwhm_scale = BASE_PSF_FWHM_SCALE * resolutionScale; // 缩放 PSF_FWHM_SCALE

    for (const auto& star : stars) {
        // 只有亮度高于阈值的恒星才会被绘制
        if (star.magnitude > magnitudeThreshold) {
            continue;
        }

        double deltaRA = star.ra - centerRA;
        deltaRA = std::fmod(deltaRA + 180.0, 360.0) - 180.0;

        bool inRA = (std::abs(deltaRA) <= fovRA) / 2.0;
        bool inDec = (star.dec >= (centerDec - fovDec/2.0)) &&
                     (star.dec <= (centerDec + fovDec/2.0));

        if (!(inRA && inDec)) continue;

        double normalizedRA = (deltaRA + fovRA/2.0) / fovRA;
        double normalizedDec = (centerDec + fovDec/2.0 - star.dec) / fovDec;

        double x = normalizedRA * imageWidth;
        double y = normalizedDec * imageHeight;

        // 添加微小的随机位置抖动
        double jitterX = jitter(gen);
        double jitterY = jitter(gen);
        int centerX = std::round(x + jitterX);
        int centerY = std::round(y + jitterY);

        // 计算恒星的强度和半径
        double intensity = intensityFromMagnitude(star.magnitude);
        double radius = std::clamp(intensity * current_max_radius, MIN_RADIUS, current_max_radius);

        // 修改亮度计算方式，考虑饱和星等
        double baseBrightness;
        if (star.magnitude <= SATURATION_MAGNITUDE) {
            // 对于非常亮的星，使用更高的基础亮度来模拟饱和
            baseBrightness = BASE_LUMINOSITY * 3.0; // 可以调整这个倍数
        } else {
            double brightnessFactor = 1.0 - (star.magnitude / magnitudeThreshold);
            brightnessFactor = std::max(0.0, std::min(1.0, brightnessFactor));
            baseBrightness = BASE_LUMINOSITY * std::pow(brightnessFactor, 2.0);
        }

        // 优化PSF参数
        const double beta = PSF_BETA;
        const double originalFWHM = current_psf_fwhm_scale * radius; // 使用缩放后的 PSF_FWHM_SCALE
        const double sqrt_term = std::sqrt(std::pow(2.0, 1.0/beta) - 1.0);
        const double alpha = originalFWHM / (2.0 * sqrt_term);

        // 扩大采样范围 (可能需要根据 PSF 的缩放调整)
        int kernelSize = static_cast<int>(std::ceil(PSF_KERNEL_SIZE_MULTIPLIER * alpha));

        std::vector<std::tuple<int, int, double>> psfFactors;
        double sumFactors = 0.0;

        // 预计算PSF核
        for (int dy = -kernelSize; dy <= kernelSize; ++dy) {
            for (int dx = -kernelSize; dx <= kernelSize; ++dx) {
                double r2 = dx*dx + dy*dy;
                double factor = std::pow(1.0 + r2/(alpha*alpha), -beta);
                psfFactors.emplace_back(dx, dy, factor);
                sumFactors += factor;
            }
        }

        // 应用PSF
        const double normalization = 1.0 / sumFactors;
        for (const auto& [dx, dy, factor] : psfFactors) {
            int px = centerX + dx;
            int py = centerY + dy;
            if (px < 0 || px >= imageWidth || py < 0 || py >= imageHeight) continue;

            // 添加微小的随机亮度噪声
            double brightnessWithNoise = baseBrightness * (1.0 + brightness_noise(gen));
            float value = static_cast<float>(brightnessWithNoise * factor * normalization);
            cv::Vec3f& pixel = accumulationBuffer.at<cv::Vec3f>(py, px);

            // 对于非常亮的星，允许像素值超过 255，在后续的归一化和后处理中会处理饱和效果
            pixel[0] += value;
            pixel[1] += value;
            pixel[2] += value;
        }
    }

    // 优化后处理流程
    // 优化后处理流程
    cv::Mat outputImage;

    // 自适应直方图均衡化
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(2.0);

    std::vector<cv::Mat> channels;
    cv::split(accumulationBuffer, channels);
    for (auto& channel : channels) {
        // 先将 float 类型的 channel 归一化到 0-255 并转换为 8-bit unsigned
        cv::Mat normalizedChannel;
        cv::normalize(channel, normalizedChannel, 0, 255, cv::NORM_MINMAX, CV_8U);
        // 然后对转换后的通道应用 CLAHE
        clahe->apply(normalizedChannel, channel); // 注意这里我们将结果写回 channel
    }
    cv::merge(channels, outputImage);

    // 保存结果
    std::filesystem::path dir = std::filesystem::path(outputPath).parent_path();
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    if (!cv::imwrite(outputPath, outputImage)) {
        std::cerr << "Error: Could not save the star map to " << outputPath << std::endl;
    } else {
        std::cout << "Star map saved to " << outputPath << std::endl;
    }
}

}