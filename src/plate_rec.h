// 车牌矫正 + 识别 + 颜色
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "infer/engine.h"

namespace pfd {

enum PlateColor { PC_UNKNOWN = -1, PC_BLUE = 0, PC_YELLOW, PC_GREEN, PC_WHITE, PC_BLACK, PC_NUM };
const char* plate_color_cn(int c);   // "蓝" ...
const char* plate_color_en(int c);   // "blue" ...

struct PlateRecResult {
    std::string text;        // UTF-8
    float conf = 0.f;        // 字符平均置信度
    int color = PC_UNKNOWN;
    float color_conf = 0.f;
    bool valid_format = false;
    cv::Mat rectified;       // 拉正后的车牌小图（双层牌为拼接前）
};

struct PlateRecConfig {
    float expand_x = 0.02f;      // 四角点外扩比例（水平，占车牌宽）
    float expand_y = 0.04f;      // （垂直，占车牌高）
    bool double_merge = true;    // 双层牌: 上下切开左右拼接后一次识别；false 则上下分别识别再拼字符串
};

class PlateRecognizer {
public:
    bool load(const std::string& rec_model, const std::string& cls_model /*可为空*/);
    // kps: 4 个角点（左上、右上、右下、左下），layer: 0 单层 1 双层
    PlateRecResult recognize(const cv::Mat& frame, const std::vector<cv::Point2f>& kps, int layer,
                             double* t_rec = nullptr, double* t_cls = nullptr);
    PlateRecConfig cfg;
    bool has_cls() const { return (bool)cls_; }

    static cv::Mat rectify(const cv::Mat& frame, const std::vector<cv::Point2f>& kps, float ex, float ey);
    static cv::Mat merge_double(const cv::Mat& plate);
    static bool check_format(const std::string& utf8);

private:
    std::pair<std::string, float> run_rec(const cv::Mat& plate);
    std::unique_ptr<InferEngine> rec_, cls_;
    cv::Mat rec_in_, cls_in_;
};

int utf8_len(const std::string& s);

}  // namespace pfd
