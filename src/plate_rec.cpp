#include "plate_rec.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <opencv2/imgproc.hpp>
#include "timer.h"
#include "filters.h"

namespace pfd {

// HyperLPR3 rpv3_mdict 字典（index 0 = CTC blank）
static const char* kTokens[] = {
    "", "'", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F", "G", "H", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "云", "京", "冀", "吉", "学", "宁",
    "川", "挂", "新", "晋", "桂", "民", "沪", "津", "浙", "渝", "港", "湘", "琼", "甘", "皖", "粤", "航", "苏", "蒙", "藏", "警", "豫",
    "贵", "赣", "辽", "鄂", "闽", "陕", "青", "鲁", "黑", "领", "使", "澳"};
static const int kNumTokens = sizeof(kTokens) / sizeof(kTokens[0]);

const char* plate_color_cn(int c) {
    static const char* n[] = {"蓝", "黄", "绿", "白", "黑", "黄绿"};
    return (c >= 0 && c < PC_NUM) ? n[c] : "未知";
}
const char* plate_color_en(int c) {
    static const char* n[] = {"blue", "yellow", "green", "white", "black", "yellowgreen"};
    return (c >= 0 && c < PC_NUM) ? n[c] : "unknown";
}

int utf8_len(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

static std::vector<std::string> utf8_split(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        size_t l = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        out.push_back(s.substr(i, l)); i += l;
    }
    return out;
}

// 粗略格式校验：首字为省份/特殊汉字，总长 7(普通) 或 8(新能源)
bool PlateRecognizer::check_format(const std::string& s) {
    auto ch = utf8_split(s);
    if (ch.size() < 7 || ch.size() > 8) return false;
    static const std::string prov = "京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼使";
    if (ch[0].size() != 3 || prov.find(ch[0]) == std::string::npos) {
        if (!(ch[0] == "W" && ch[1] == "J")) return false;
    }
    if (ch[1].size() != 1 || !(ch[1][0] >= 'A' && ch[1][0] <= 'Z')) return false;
    return true;
}

bool PlateRecognizer::load(const std::string& rec_model, const std::string& cls_model) {
    NormConfig rn;  // rec: BGR, (x-127.5)/127.5
    for (int i = 0; i < 3; ++i) { rn.mean[i] = 127.5f; rn.std[i] = 127.5f; }
    rec_ = create_engine();
    if (!rec_->load(rec_model, rn)) return false;
    if (!cls_model.empty()) {
        NormConfig cn;  // cls: BGR /255
        cls_ = create_engine();
        if (!cls_->load(cls_model, cn)) { cls_.reset(); return false; }
    }
    return true;
}

cv::Mat PlateRecognizer::rectify(const cv::Mat& frame, const std::vector<cv::Point2f>& kps, float ex, float ey) {
    if (kps.size() != 4) return cv::Mat();
    // 在车牌自身坐标系里外扩：水平方向沿 (tr-tl)，垂直方向沿 (bl-tl)
    cv::Point2f c = (kps[0] + kps[1] + kps[2] + kps[3]) * 0.25f;
    cv::Point2f ux = ((kps[1] - kps[0]) + (kps[2] - kps[3])) * 0.5f;
    cv::Point2f uy = ((kps[3] - kps[0]) + (kps[2] - kps[1])) * 0.5f;
    std::vector<cv::Point2f> p(4);
    const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
    for (int i = 0; i < 4; ++i) p[i] = kps[i] + ux * (sx[i] * ex * 0.5f) + uy * (sy[i] * ey * 0.5f);
    (void)c;
    float w = std::max(cv::norm(p[0] - p[1]), cv::norm(p[2] - p[3]));
    float h = std::max(cv::norm(p[0] - p[3]), cv::norm(p[1] - p[2]));
    int W = std::max(8, (int)std::round(w)), H = std::max(4, (int)std::round(h));
    std::vector<cv::Point2f> dst = {{0, 0}, {(float)W, 0}, {(float)W, (float)H}, {0, (float)H}};
    cv::Mat M = cv::getPerspectiveTransform(p, dst);
    cv::Mat out;
    cv::warpPerspective(frame, out, M, cv::Size(W, H), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    if (out.rows >= out.cols * 1.5) cv::rotate(out, out, cv::ROTATE_90_COUNTERCLOCKWISE);
    return out;
}

// 双层牌：上 5/12 为省份+字母，下 2/3 为序号；上半 resize 成下半尺寸后左右拼接成单行
cv::Mat PlateRecognizer::merge_double(const cv::Mat& img) {
    int h = img.rows, w = img.cols;
    cv::Mat up = img(cv::Rect(0, 0, w, std::max(1, h * 5 / 12))).clone();
    cv::Mat down = img(cv::Rect(0, h / 3, w, h - h / 3)).clone();
    cv::resize(up, up, down.size());
    cv::Mat m;
    cv::hconcat(up, down, m);
    return m;
}

std::pair<std::string, float> PlateRecognizer::run_rec(const cv::Mat& plate) {
    const int H = rec_->input_h(), W = rec_->input_w();
    // 等比缩放到高 H，宽不超过 W，右侧用 128（归一化后≈0）填充，与 HyperLPR3 训练时一致
    float ratio = (float)plate.cols / plate.rows;
    int rw = std::min(W, std::max(1, (int)std::ceil(H * ratio)));
    if (rec_in_.empty()) rec_in_.create(H, W, CV_8UC3);
    rec_in_.setTo(cv::Scalar(128, 128, 128));
    cv::resize(plate, rec_in_(cv::Rect(0, 0, rw, H)), cv::Size(rw, H));
    std::vector<Tensor> outs;
    if (!rec_->run(rec_in_, outs) || outs.empty()) return {"", 0.f};
    const Tensor& t = outs[0];  // [1, T, C]
    int T = t.shape.size() == 3 ? t.shape[1] : t.shape[t.shape.size() - 2];
    int C = t.shape.back();
    std::string s; float sum = 0; int n = 0, prev = -1;
    for (int i = 0; i < T; ++i) {
        const float* p = t.data + (size_t)i * C;
        int best = (int)(std::max_element(p, p + C) - p);
        if (best != 0 && best != prev && best < kNumTokens) { s += kTokens[best]; sum += p[best]; ++n; }
        prev = best;
    }
    return {s, n ? sum / n : 0.f};
}

std::pair<std::string, float> PlateRecognizer::rec_layer(const cv::Mat& rect, int layer) {
    std::pair<std::string, float> o;
    if (layer == 1) {
        if (cfg.double_merge) o = run_rec(merge_double(rect));
        else {
            int h = rect.rows, w = rect.cols, line = (int)(h * 0.4f);
            auto a = run_rec(rect(cv::Rect(0, 0, w, line)));
            auto b = run_rec(rect(cv::Rect(0, line, w, h - line)));
            o = {a.first + b.first, (a.second + b.second) * 0.5f};
        }
    } else {
        o = run_rec(rect);
    }
    // 去掉偶发的 "'" 字符
    o.first.erase(std::remove(o.first.begin(), o.first.end(), '\''), o.first.end());
    return o;
}

void PlateRecognizer::classify_color(const cv::Mat& rect, int& color, float& conf) {
    color = PC_UNKNOWN; conf = 0.f;
    // HSV 统计（中心区域）
    cv::Mat hsv; cv::Mat center = rect(cv::Rect(rect.cols / 8, rect.rows / 6, std::max(1, rect.cols * 3 / 4), std::max(1, rect.rows * 2 / 3)));
    cv::cvtColor(center, hsv, cv::COLOR_BGR2HSV);
    cv::Scalar m = cv::mean(hsv);
    float sat = (float)m[1], val = (float)m[2];
    if (cls_) {
        const int CH = cls_->input_h(), CW = cls_->input_w();
        cv::resize(rect, cls_in_, cv::Size(CW, CH));
        std::vector<Tensor> outs;
        if (cls_->run(cls_in_, outs) && !outs.empty() && outs[0].size() >= 3) {
            const float* p = outs[0].data;   // [blue, green, yellow]，已 softmax
            int b = (int)(std::max_element(p, p + 3) - p);
            static const int map3[3] = {PC_BLUE, PC_GREEN, PC_YELLOW};
            color = map3[b]; conf = p[b];
        }
    }
    // 低饱和度的牌，分类网络（只有 3 类）不可信 -> 白/黑
    if (sat < 45.f && (color == PC_UNKNOWN || conf < 0.9f)) {
        color = val > 110.f ? PC_WHITE : PC_BLACK; conf = 0.5f;
    } else if (color == PC_UNKNOWN) {
        double hue = m[0];  // OpenCV H: 0~180
        color = (hue > 90 && hue < 130) ? PC_BLUE : (hue > 15 && hue < 35) ? PC_YELLOW : (hue >= 35 && hue <= 90) ? PC_GREEN : PC_BLUE;
        conf = 0.4f;
    }
}

PlateRecResult PlateRecognizer::recognize(const cv::Mat& frame, const std::vector<cv::Point2f>& kps, int layer,
                                          double* t_rec, double* t_cls) {
    Timer tm;
    PlateRecResult r;
    r.rectified = rectify(frame, kps, cfg.expand_x, cfg.expand_y);
    if (r.rectified.empty()) return r;
    r.det_layer = layer; r.layer = layer;
    double tcls = 0;
    float pre_conf = 0.f;
    // ---- v2.1: 检测器判双层时，先按拉正后整块车牌分类颜色，只有允许双层的颜色才按双层切分拼接 ----
    if (layer == 1 && cfg.layer_by_color) {
        Timer tc;
        classify_color(r.rectified, r.pre_color, pre_conf);
        tcls += tc.lap();
        bool allowed = std::find(cfg.double_colors.begin(), cfg.double_colors.end(), r.pre_color) != cfg.double_colors.end();
        if (!allowed) { r.layer = 0; r.layer_overridden = true; }
    }
    Timer trc;
    auto o = rec_layer(r.rectified, r.layer);
    r.text = o.first; r.conf = o.second;
    r.valid_format = check_format(r.text);
    if (r.layer_overridden && cfg.layer_fallback && !r.valid_format && pre_conf < cfg.layer_fallback_conf) {
        // 颜色可能判错（如暗光黄牌判成绿/蓝）：按双层再试一次，合法才采用
        auto o2 = rec_layer(r.rectified, 1);
        if (check_format(o2.first)) { r.text = o2.first; r.conf = o2.second; r.valid_format = true; r.layer = 1; r.layer_overridden = false; }
    }
    // ---- v2.4: 单/双层双假设：当前假设结果不够可信时，再按另一种层数识别，择优 ----
    if (cfg.dual_hyp) {
        auto fmt_score = [&](const std::string& t) { return strict_plate_format(t, cfg.max_serial_letters) ? 2 : (check_format(t) ? 1 : 0); };
        auto is_dbl_color = [&](int c) { return std::find(cfg.double_colors.begin(), cfg.double_colors.end(), c) != cfg.double_colors.end(); };
        int cur_fs = fmt_score(r.text);
        bool confident = cur_fs == 2 && r.conf >= cfg.dual_hyp_conf;
        if (!confident) {
            int other = -1;
            if (r.layer == 1) other = 0;                       // 当前双层 -> 试单层（任何颜色都允许单层）
            else if (cfg.dual_hyp_from_single && r.layer == 0) {
                // 当前单层（检测器判单层，或判双层但被颜色改成单层）-> 试双层，须判层颜色允许双层
                if (!cfg.layer_by_color) other = 1;
                else {
                    if (r.pre_color == PC_UNKNOWN) { Timer tc; classify_color(r.rectified, r.pre_color, pre_conf); tcls += tc.lap(); }
                    if (is_dbl_color(r.pre_color)) other = 1;
                }
            }
            if (other >= 0) {
                auto o2 = rec_layer(r.rectified, other);
                int fs2 = fmt_score(o2.first);
                bool take = fs2 > cur_fs || (fs2 == cur_fs && fs2 > 0 && o2.second > r.conf + cfg.dual_hyp_margin);
                r.dual_tried = true;
                if (take) {
                    r.text = o2.first; r.conf = o2.second; r.valid_format = check_format(r.text);
                    r.dual_switched = true; r.layer = other; r.layer_overridden = (other == 0 && layer == 1);
                }
            }
        }
    }
    double trec = trc.lap();

    // ---- 颜色: 规则(字符) > 分类网络(蓝/绿/黄) > HSV 兜底(白/黑) ----
    Timer tc2;
    const std::string& s = r.text;
    auto has = [&](const char* k) { return s.find(k) != std::string::npos; };
    int n = utf8_len(s);
    if (s.rfind("WJ", 0) == 0 || has("警")) { r.color = PC_WHITE; r.color_conf = 0.9f; r.color_src = "rule"; }
    else if (has("港") || has("澳") || has("使") || has("领") || s.rfind("粤Z", 0) == 0) { r.color = PC_BLACK; r.color_conf = 0.9f; r.color_src = "rule"; }
    else if (has("学") || has("挂")) { r.color = PC_YELLOW; r.color_conf = 0.9f; r.color_src = "rule"; }
    else if (n == 8 && r.valid_format) {
        // 8 位新能源：默认绿；v2.3 黄绿规则需要分类颜色，故先跑分类
        if (cfg.yellowgreen_rule) {
            if (r.pre_color != PC_UNKNOWN) r.cls_color = r.pre_color;
            else { float cc; classify_color(r.rectified, r.cls_color, cc); }
        }
        r.color = PC_GREEN; r.color_conf = 0.9f; r.color_src = "rule";
    }
    else if (r.pre_color != PC_UNKNOWN) { r.color = r.pre_color; r.color_conf = pre_conf; }   // 已分类过，复用
    else classify_color(r.rectified, r.color, r.color_conf);
    if (r.cls_color == PC_UNKNOWN && std::string(r.color_src) == "cls") r.cls_color = r.color;   // 规则色（白/黑/黄）时 cls_color 留空
    if (cfg.yellowgreen_rule && n == 8) {
        int c2 = resolve_plate_color(s, r.color, r.cls_color, cfg);
        if (c2 == PC_YELLOWGREEN) { r.color = PC_YELLOWGREEN; r.color_conf = 0.9f; r.color_src = "rule"; }
    }
    tcls += tc2.lap();
    (void)tm;
    if (t_rec) *t_rec = trec;
    if (t_cls) *t_cls = tcls;
    return r;
}

int resolve_plate_color(const std::string& text, int color, int cls_color, const PlateRecConfig& cfg, const char** src) {
    if (src) *src = "cls";
    if (!cfg.yellowgreen_rule) return color == PC_YELLOWGREEN ? PC_GREEN : color;
    bool large = is_large_ne_plate(text, cfg.yellowgreen_letters, cfg.max_serial_letters);
    if (large) {
        bool ok = std::find(cfg.yellowgreen_colors.begin(), cfg.yellowgreen_colors.end(), cls_color) != cfg.yellowgreen_colors.end();
        if (ok) { if (src) *src = "rule"; return PC_YELLOWGREEN; }
        return color == PC_YELLOWGREEN ? PC_GREEN : color;
    }
    if (color == PC_YELLOWGREEN) { if (src) *src = "rule"; return PC_GREEN; }   // 号码不是大型新能源 -> 退回绿
    return color;
}

}  // namespace pfd
