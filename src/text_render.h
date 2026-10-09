// 预渲染字形贴图（UTF-8，含车牌省份汉字），板端无需 FreeType/字体文件
#pragma once
#include <string>
#include <opencv2/core.hpp>
namespace pfd {
// 返回绘制后的文字宽高；org 为左上角；height 为字高(px)
cv::Size text_size(const std::string& utf8, int height);
cv::Size put_text(cv::Mat& img, const std::string& utf8, cv::Point org, int height, const cv::Scalar& color,
                  const cv::Scalar* bg = nullptr, int pad = 2);
}  // namespace pfd
