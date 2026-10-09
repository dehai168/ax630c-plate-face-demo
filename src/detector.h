// YOLOv5-face 风格「框 + 关键点 + 多类」检测器（人脸: 5 点 1 类；车牌: 4 角点 2 类 单/双层）
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "infer/engine.h"

namespace pfd {

struct Detection {
    cv::Rect2f box;                  // 原图像素坐标
    float score = 0.f;
    int cls = 0;
    std::vector<cv::Point2f> kps;    // 原图像素坐标
};

struct LetterboxInfo { float scale = 1.f; int pad_x = 0, pad_y = 0; };

class YoloKpDetector {
public:
    YoloKpDetector(int num_kps, int num_cls, std::vector<float> anchors /*3 层 x 3 anchor x (w,h)*/);
    bool load(const std::string& model, const NormConfig& norm);
    // 返回检测结果；内部耗时（前处理/推理/后处理）写入 t_pre/t_inf/t_post (ms)
    std::vector<Detection> detect(const cv::Mat& bgr, float conf_thr, float nms_thr,
                                  double* t_pre = nullptr, double* t_inf = nullptr, double* t_post = nullptr);
    int input_w() const { return engine_ ? engine_->input_w() : 0; }
    int input_h() const { return engine_ ? engine_->input_h() : 0; }
    std::string backend() const { return engine_ ? engine_->backend() : ""; }
    int pad_value = 114;   // letterbox 填充值（人脸 114，HyperLPR3 车牌检测训练/推理用 0）

private:
    void decode(const Tensor& t, int stride, int level, float conf_thr, const LetterboxInfo& lb,
                std::vector<Detection>& out) const;
    std::unique_ptr<InferEngine> engine_;
    int nk_, nc_, no_;
    std::vector<float> anchors_;
    cv::Mat lb_img_;
};

void letterbox(const cv::Mat& src, cv::Mat& dst, int w, int h, LetterboxInfo& info, int pad_value = 114);
void nms(std::vector<Detection>& dets, float iou_thr);
float iou(const cv::Rect2f& a, const cv::Rect2f& b);

}  // namespace pfd
