#include "detector.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <opencv2/imgproc.hpp>
#include "timer.h"

namespace pfd {

static inline float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

YoloKpDetector::YoloKpDetector(int num_kps, int num_cls, std::vector<float> anchors)
    : nk_(num_kps), nc_(num_cls), no_(5 + 2 * num_kps + num_cls), anchors_(std::move(anchors)) {}

bool YoloKpDetector::load(const std::string& model, const NormConfig& norm) {
    engine_ = create_engine();
    return engine_->load(model, norm);
}

void letterbox(const cv::Mat& src, cv::Mat& dst, int w, int h, LetterboxInfo& info, int pad_value) {
    float r = std::min((float)w / src.cols, (float)h / src.rows);
    int nw = (int)std::round(src.cols * r), nh = (int)std::round(src.rows * r);
    info.scale = r;
    info.pad_x = (w - nw) / 2;
    info.pad_y = (h - nh) / 2;
    if (dst.rows != h || dst.cols != w || dst.type() != CV_8UC3) dst.create(h, w, CV_8UC3);
    dst.setTo(cv::Scalar(pad_value, pad_value, pad_value));
    cv::Mat roi = dst(cv::Rect(info.pad_x, info.pad_y, nw, nh));
    cv::resize(src, roi, cv::Size(nw, nh), 0, 0, cv::INTER_LINEAR);
}

float iou(const cv::Rect2f& a, const cv::Rect2f& b) {
    float inter = (a & b).area();
    float uni = a.area() + b.area() - inter;
    return uni > 0 ? inter / uni : 0.f;
}

void nms(std::vector<Detection>& dets, float iou_thr) {
    std::sort(dets.begin(), dets.end(), [](const Detection& a, const Detection& b) { return a.score > b.score; });
    std::vector<Detection> keep;
    std::vector<bool> removed(dets.size(), false);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (removed[i]) continue;
        keep.push_back(dets[i]);
        for (size_t j = i + 1; j < dets.size(); ++j)
            if (!removed[j] && iou(dets[i].box, dets[j].box) > iou_thr) removed[j] = true;
    }
    dets.swap(keep);
}

// 支持 NCHW [1, na*no, H, W] 和 NHWC [1, H, W, na*no] 两种输出布局
void YoloKpDetector::decode(const Tensor& t, int stride, int level, float conf_thr, const LetterboxInfo& lb,
                            std::vector<Detection>& out) const {
    const int na = 3, C = na * no_;
    const int gh = input_h() / stride, gw = input_w() / stride;
    bool nchw;
    if (t.shape.size() == 4 && t.shape[1] == C && t.shape[2] == gh && t.shape[3] == gw) nchw = true;
    else if (t.shape.size() == 4 && t.shape[3] == C && t.shape[1] == gh && t.shape[2] == gw) nchw = false;
    else { fprintf(stderr, "[det] unexpected output shape for stride %d\n", stride); return; }
    const float* d = t.data;
    const int plane = gh * gw;
    auto at = [&](int a, int k, int y, int x) -> float {
        int c = a * no_ + k;
        return nchw ? d[(size_t)c * plane + y * gw + x] : d[((size_t)y * gw + x) * C + c];
    };
    // obj 的 logit 阈值，先粗筛减少 exp 计算
    const float obj_logit_thr = -std::log(1.f / std::max(conf_thr, 1e-4f) - 1.f);
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
            for (int a = 0; a < na; ++a) {
                float obj_l = at(a, 4, y, x);
                if (obj_l < obj_logit_thr) continue;
                float obj = sigmoid(obj_l);
                int best_c = 0; float best = -1e9f;
                for (int c = 0; c < nc_; ++c) {
                    float v = at(a, 5 + 2 * nk_ + c, y, x);
                    if (v > best) { best = v; best_c = c; }
                }
                float score = obj * sigmoid(best);
                if (score < conf_thr) continue;
                float aw = anchors_[level * 6 + a * 2], ah = anchors_[level * 6 + a * 2 + 1];
                float cx = (sigmoid(at(a, 0, y, x)) * 2.f - 0.5f + x) * stride;
                float cy = (sigmoid(at(a, 1, y, x)) * 2.f - 0.5f + y) * stride;
                float bw = std::pow(sigmoid(at(a, 2, y, x)) * 2.f, 2.f) * aw;
                float bh = std::pow(sigmoid(at(a, 3, y, x)) * 2.f, 2.f) * ah;
                Detection det;
                det.score = score; det.cls = best_c;
                float x0 = (cx - bw / 2 - lb.pad_x) / lb.scale, y0 = (cy - bh / 2 - lb.pad_y) / lb.scale;
                float x1 = (cx + bw / 2 - lb.pad_x) / lb.scale, y1 = (cy + bh / 2 - lb.pad_y) / lb.scale;
                det.box = cv::Rect2f(x0, y0, x1 - x0, y1 - y0);
                for (int k = 0; k < nk_; ++k) {
                    // yolov5-face: 关键点 = raw * anchor + grid * stride（不过 sigmoid）
                    float kx = at(a, 5 + 2 * k, y, x) * aw + x * stride;
                    float ky = at(a, 5 + 2 * k + 1, y, x) * ah + y * stride;
                    det.kps.emplace_back((kx - lb.pad_x) / lb.scale, (ky - lb.pad_y) / lb.scale);
                }
                out.push_back(std::move(det));
            }
}

std::vector<Detection> YoloKpDetector::detect(const cv::Mat& bgr, float conf_thr, float nms_thr,
                                              double* t_pre, double* t_inf, double* t_post) {
    Timer tm;
    LetterboxInfo lb;
    letterbox(bgr, lb_img_, input_w(), input_h(), lb, pad_value);
    if (t_pre) *t_pre = tm.lap();
    std::vector<Tensor> outs;
    std::vector<Detection> dets;
    if (!engine_->run(lb_img_, outs)) return dets;
    if (t_inf) *t_inf = tm.lap();
    // 输出按特征图大小排序：stride 8,16,32
    std::vector<std::pair<int, const Tensor*>> lv;
    for (auto& o : outs) {
        int area = (int)(o.size() / (3 * no_));
        lv.emplace_back(area, &o);
    }
    std::sort(lv.begin(), lv.end(), [](auto& a, auto& b) { return a.first > b.first; });
    const int strides[3] = {8, 16, 32};
    for (int i = 0; i < (int)lv.size() && i < 3; ++i) decode(*lv[i].second, strides[i], i, conf_thr, lb, dets);
    nms(dets, nms_thr);
    // 裁到图像范围内
    cv::Rect2f img_r(0, 0, (float)bgr.cols, (float)bgr.rows);
    for (auto& d : dets) d.box &= img_r;
    dets.erase(std::remove_if(dets.begin(), dets.end(), [](const Detection& d) { return d.box.area() <= 1.f; }), dets.end());
    if (t_post) *t_post = tm.lap();
    return dets;
}

}  // namespace pfd
