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
    int det_layer = 0;       // 检测器给出的层数（0 单 1 双）
    int layer = 0;           // 实际采用的层数（按颜色规则修正后）
    bool layer_overridden = false;  // 检测器判双层、但颜色不允许双层 -> 按单层识别
    int pre_color = PC_UNKNOWN;     // 判层用的颜色（识别前在拉正图上分类）
};

struct PlateRecConfig {
    float expand_x = 0.02f;      // 四角点外扩比例（水平，占车牌宽）
    float expand_y = 0.04f;      // （垂直，占车牌高）
    bool double_merge = true;    // 双层牌: 上下切开左右拼接后一次识别；false 则上下分别识别再拼字符串
    // v2.1: 层数由颜色决定。检测器判双层时，先在拉正图上做颜色分类，颜色不在 double_colors 内则按单层处理
    bool layer_by_color = true;
    std::vector<int> double_colors = {PC_YELLOW};   // 允许双层的颜色（默认只有黄牌）
    bool layer_fallback = true;  // 被改成单层后识别结果格式不合法、且判层颜色置信度低于 layer_fallback_conf 时，再按双层试一次，取合法者
    float layer_fallback_conf = 0.7f;
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
    std::pair<std::string, float> rec_layer(const cv::Mat& rect, int layer);
    void classify_color(const cv::Mat& rect, int& color, float& conf);   // 分类网络 + HSV 兜底
    std::unique_ptr<InferEngine> rec_, cls_;
    cv::Mat rec_in_, cls_in_;
};

int utf8_len(const std::string& s);

}  // namespace pfd
