// 车牌矫正 + 识别 + 颜色
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "infer/engine.h"

namespace pfd {

enum PlateColor { PC_UNKNOWN = -1, PC_BLUE = 0, PC_YELLOW, PC_GREEN, PC_WHITE, PC_BLACK, PC_YELLOWGREEN, PC_NUM };   // 黄绿 = 大型新能源（规则判定）
const char* plate_color_cn(int c);   // "蓝" ...
const char* plate_color_en(int c);   // "blue" ...

struct PlateRecResult {
    std::string text;        // UTF-8
    float conf = 0.f;        // 字符平均置信度
    int color = PC_UNKNOWN;
    float color_conf = 0.f;
    int cls_color = PC_UNKNOWN;   // v2.3: 颜色分类网络（含 HSV 兜底）的结果
    const char* color_src = "cls";   // cls = 分类网络/HSV；rule = 由号码规则决定
    bool valid_format = false;
    cv::Mat rectified;       // 拉正后的车牌小图（双层牌为拼接前）
    int det_layer = 0;       // 检测器给出的层数（0 单 1 双）
    int layer = 0;           // 实际采用的层数（按颜色规则修正后）
    bool dual_tried = false;        // v2.4: 做了双假设识别
    bool dual_switched = false;     // v2.4: 双假设改变了层数
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
    // v2.3: 黄绿（大型新能源）规则：8 位合法、末位为 yellowgreen_letters 之一、且分类颜色在 yellowgreen_colors 内 -> 黄绿
    bool yellowgreen_rule = true;
    std::string yellowgreen_letters = "ABCDEFGHJK";
    std::vector<int> yellowgreen_colors = {PC_GREEN, PC_YELLOW};
    int max_serial_letters = 3;
    // v2.4: 单/双层双假设识别。不确定时按单层、双层各识别一次，按格式合法性 + 置信度择优。
    // 双层假设仍只对 double_colors（默认黄牌）开放（识别前的判层颜色为黄，或颜色未判且检测器说双层）。
    bool dual_hyp = true;
    float dual_hyp_conf = 0.95f;     // 当前假设的结果已“严格合法且置信度 >= 此值”则不再试另一种（省一次推理）
    float dual_hyp_margin = 0.05f;   // 两种假设都合法时，另一假设置信度须高出此值才取代当前
    bool dual_hyp_from_single = true;// 检测器判单层、结果不合法、拉正图分类为允许双层的颜色 -> 也试双层
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
// v2.3: 由最终号码 + 颜色得到最终颜色（黄绿规则）；src 输出 rule/cls
// color: 当前（投票）颜色；cls_color: 分类网络颜色（投票）。大型新能源号码且 cls_color 在 yellowgreen_colors 内 -> 黄绿
int resolve_plate_color(const std::string& text, int color, int cls_color, const PlateRecConfig& cfg, const char** src = nullptr);

}  // namespace pfd
