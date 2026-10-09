#include "filters.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <opencv2/imgproc.hpp>

namespace pfd {

std::vector<std::string> utf8_chars(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        size_t l = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        out.push_back(s.substr(i, l)); i += l;
    }
    return out;
}

int utf8_edit_distance(const std::string& a, const std::string& b) {
    auto x = utf8_chars(a), y = utf8_chars(b);
    std::vector<int> d(y.size() + 1);
    for (size_t j = 0; j <= y.size(); ++j) d[j] = (int)j;
    for (size_t i = 1; i <= x.size(); ++i) {
        int prev = d[0]; d[0] = (int)i;
        for (size_t j = 1; j <= y.size(); ++j) {
            int cur = d[j];
            d[j] = std::min({d[j] + 1, d[j - 1] + 1, prev + (x[i - 1] != y[j - 1])});
            prev = cur;
        }
    }
    return d[y.size()];
}

static bool is_letter(const std::string& c) { return c.size() == 1 && c[0] >= 'A' && c[0] <= 'Z' && c[0] != 'I' && c[0] != 'O'; }
static bool is_digit(const std::string& c) { return c.size() == 1 && c[0] >= '0' && c[0] <= '9'; }
static bool is_alnum(const std::string& c) { return is_letter(c) || is_digit(c); }
static bool in_set(const std::string& c, const char* set) {
    std::string s(set);
    return c.size() == 3 && s.find(c) != std::string::npos;
}

bool strict_plate_format(const std::string& s, int max_letters) {
    auto ch = utf8_chars(s);
    const char* prov = "京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼";
    if (ch.size() < 7 || ch.size() > 8) return false;
    // 武警 WJ
    if (ch[0] == "W" && ch[1] == "J") {
        for (size_t i = 2; i < ch.size(); ++i) if (!is_alnum(ch[i]) && !(i + 1 == ch.size() && in_set(ch[i], "警"))) return false;
        return true;
    }
    if (!in_set(ch[0], prov) || !is_letter(ch[1])) return false;
    auto nev = [](const std::string& c) { return c.size() == 1 && std::string("DABCEFGHJK").find(c[0]) != std::string::npos; };
    if (ch.size() == 8) {
        // 小型新能源: 第 3 位 D/F 等 + 1 位字母数字 + 4 位数字；大型: 5 位数字 + D/F 等
        bool small = nev(ch[2]) && is_alnum(ch[3]) && is_digit(ch[4]) && is_digit(ch[5]) && is_digit(ch[6]) && is_digit(ch[7]);
        bool large = is_digit(ch[2]) && is_digit(ch[3]) && is_digit(ch[4]) && is_digit(ch[5]) && is_digit(ch[6]) && nev(ch[7]);
        return small || large;
    }
    // 7 位普通牌
    int letters = 0;
    for (int i = 2; i <= 5; ++i) { if (!is_alnum(ch[i])) return false; letters += is_letter(ch[i]); }
    if (is_alnum(ch[6])) letters += is_letter(ch[6]);
    else if (!in_set(ch[6], "挂学警港澳领使")) return false;
    return letters <= max_letters;
}

static float angle_deg(cv::Point2f a, cv::Point2f o, cv::Point2f b) {
    cv::Point2f u = a - o, v = b - o;
    float nu = std::sqrt(u.dot(u)), nv = std::sqrt(v.dot(v));
    if (nu < 1e-3f || nv < 1e-3f) return 0.f;
    float c = std::max(-1.f, std::min(1.f, u.dot(v) / (nu * nv)));
    return std::acos(c) * 180.f / (float)CV_PI;
}

bool plate_geometry_ok(const Detection& d, const PlateGeomConfig& cfg, std::string* reason) {
    auto fail = [&](const char* r) { if (reason) *reason = r; return false; };
    if (d.kps.size() != 4) return fail("kps");
    const auto& k = d.kps;   // 左上 右上 右下 左下
    if (d.box.width < cfg.min_width) return fail("small");
    // 角点在框附近
    cv::Rect2f ex(d.box.x - d.box.width * cfg.kps_out_box, d.box.y - d.box.height * cfg.kps_out_box,
                  d.box.width * (1 + 2 * cfg.kps_out_box), d.box.height * (1 + 2 * cfg.kps_out_box));
    for (auto& p : k) if (!ex.contains(p)) return fail("kps_out");
    // 顺序/凸性：叉积同号
    float sgn = 0;
    for (int i = 0; i < 4; ++i) {
        cv::Point2f a = k[(i + 1) % 4] - k[i], b = k[(i + 2) % 4] - k[(i + 1) % 4];
        float c = a.x * b.y - a.y * b.x;
        if (i == 0) sgn = c; else if (c * sgn <= 0) return fail("non_convex");
    }
    if (sgn <= 0) return fail("order");   // 图像坐标下顺时针 => 叉积 > 0
    float top = cv::norm(k[1] - k[0]), bot = cv::norm(k[2] - k[3]);
    float left = cv::norm(k[3] - k[0]), right = cv::norm(k[2] - k[1]);
    if (std::max(top, bot) > cfg.side_ratio_max * std::max(1.f, std::min(top, bot))) return fail("side_ratio_h");
    if (std::max(left, right) > cfg.side_ratio_max * std::max(1.f, std::min(left, right))) return fail("side_ratio_v");
    for (int i = 0; i < 4; ++i)
        if (angle_deg(k[(i + 3) % 4], k[i], k[(i + 1) % 4]) < cfg.min_angle_deg) return fail("angle");
    float w = std::max(top, bot), h = std::max(1.f, std::max(left, right));
    float ar = w / h;
    bool dbl = d.cls == 1;
    if (!dbl && (ar < cfg.single_ar_min || ar > cfg.single_ar_max)) return fail("aspect");
    if (dbl && (ar < cfg.double_ar_min || ar > cfg.double_ar_max)) return fail("aspect");
    return true;
}

float face_geometry_score(const Detection& d, const FaceGeomConfig& cfg, std::string* reason) {
    auto fail = [&](const char* r) { if (reason) *reason = r; return -1.f; };
    if (std::min(d.box.width, d.box.height) < cfg.min_size) return fail("small");
    float ar = d.box.width / std::max(1.f, d.box.height);
    if (ar < 0.45f || ar > 1.4f) return fail("box_aspect");
    if (d.kps.size() != 5) return 0.5f;
    const auto& k = d.kps;   // 左眼 右眼 鼻尖 左嘴角 右嘴角（图像左/右）
    cv::Rect2f ex(d.box.x - d.box.width * cfg.kps_out_box, d.box.y - d.box.height * cfg.kps_out_box,
                  d.box.width * (1 + 2 * cfg.kps_out_box), d.box.height * (1 + 2 * cfg.kps_out_box));
    for (auto& p : k) if (!ex.contains(p)) return fail("kps_out");
    cv::Point2f le = k[0], re = k[1], nose = k[2], lm = k[3], rm = k[4];
    // 侧脸时左右点会靠拢甚至轻微交叉，只拒绝明显交叉的
    if (re.x < le.x - 0.15f * d.box.width || rm.x < lm.x - 0.15f * d.box.width) return fail("lr_order");
    float ed = (float)cv::norm(re - le);
    float edr = ed / d.box.width;
    if (edr < cfg.eye_dist_min || edr > cfg.eye_dist_max) return fail("eye_dist");
    float eye_y = (le.y + re.y) / 2, mouth_y = (lm.y + rm.y) / 2;
    float em = mouth_y - eye_y;   // 眼-嘴垂直距离
    if (em < 0.15f * d.box.height) return fail("eye_mouth");
    // 眼在上半部，嘴在下半部
    if (eye_y > d.box.y + 0.70f * d.box.height || mouth_y < d.box.y + 0.45f * d.box.height) return fail("vertical");
    // 鼻尖位于眼与嘴之间
    if (nose.y < eye_y - 0.05f * em || nose.y > mouth_y + 0.05f * em) return fail("nose_v");
    float xl = std::min(le.x, lm.x) - 0.5f * ed - 0.1f * d.box.width, xr = std::max(re.x, rm.x) + 0.5f * ed + 0.1f * d.box.width;
    if (nose.x < xl || nose.x > xr) return fail("nose_h");
    // 眼/嘴连线基本平行（倾斜 < 35°）
    float ang_e = std::atan2(re.y - le.y, re.x - le.x), ang_m = std::atan2(rm.y - lm.y, rm.x - lm.x);
    if (std::fabs(ang_e - ang_m) > 0.6f) return fail("parallel");
    // 嘴宽与眼距比例
    float mw = (float)cv::norm(rm - lm);
    if (mw > 2.5f * std::max(ed, 0.1f * d.box.width)) return fail("mouth_w");
    // 对称性得分：鼻尖相对眼中点偏移
    float off = std::fabs(nose.x - (le.x + re.x) / 2) / std::max(1.f, ed);
    float sym = std::max(0.f, 1.f - off);
    return 0.5f + 0.5f * sym;
}

float face_skin_ratio(const cv::Mat& bgr, const cv::Rect2f& box) {
    // 取框中部（去掉头发/背景），缩到 24x24 后在 YCrCb 统计肤色像素比例
    cv::Rect r((int)(box.x + box.width * 0.2f), (int)(box.y + box.height * 0.25f), (int)(box.width * 0.6f), (int)(box.height * 0.6f));
    r &= cv::Rect(0, 0, bgr.cols, bgr.rows);
    if (r.area() < 16) return 0.f;
    cv::Mat s, y;
    cv::resize(bgr(r), s, cv::Size(24, 24), 0, 0, cv::INTER_AREA);
    cv::cvtColor(s, y, cv::COLOR_BGR2YCrCb);
    int n = 0;
    for (int i = 0; i < y.rows; ++i)
        for (int j = 0; j < y.cols; ++j) {
            cv::Vec3b v = y.at<cv::Vec3b>(i, j);
            if (v[0] > 40 && v[1] >= 135 && v[1] <= 180 && v[2] >= 80 && v[2] <= 130) ++n;
        }
    return (float)n / (y.rows * y.cols);
}

// ---------------- OSD ----------------
cv::Mat OsdFilter::ring_patch(const cv::Mat& g, const cv::Rect2f& b) const {
    // 框外扩 2 倍区域（缩略图坐标），框内置 0，只看外围背景
    cv::Rect2f r(b.x * sx_, b.y * sy_, b.width * sx_, b.height * sy_);
    cv::Rect2f e(r.x - r.width * 1.0f, r.y - r.height * 2.5f, r.width * 3.f, r.height * 6.f);
    cv::Rect ri = cv::Rect((int)e.x, (int)e.y, (int)std::ceil(e.width), (int)std::ceil(e.height)) & cv::Rect(0, 0, g.cols, g.rows);
    if (ri.area() < 4) return cv::Mat();
    cv::Mat p = g(ri).clone();
    cv::Rect inner = cv::Rect((int)r.x - ri.x, (int)r.y - ri.y, (int)std::ceil(r.width), (int)std::ceil(r.height)) & cv::Rect(0, 0, p.cols, p.rows);
    if (inner.area() > 0) p(inner).setTo(0);
    // 统一尺寸，便于不同时刻比较（框位置/大小有轻微抖动）
    cv::Mat o;
    cv::resize(p, o, cv::Size(32, 24), 0, 0, cv::INTER_AREA);
    return o;
}

float OsdFilter::ring_diff(const cv::Mat& a, const cv::Mat& b) const {
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.f;
    cv::Mat d; cv::absdiff(a, b, d);
    return (float)cv::mean(d)[0];
}

void OsdFilter::update(int fi, const cv::Mat& frame, const std::vector<cv::Rect2f>& boxes) {
    if (!cfg_.enable) return;
    if (!boxes.empty()) {
        const int SW = 320;
        sx_ = (float)SW / frame.cols; sy_ = sx_;
        cv::Mat s;
        cv::resize(frame, s, cv::Size(SW, (int)std::round(frame.rows * sy_)), 0, 0, cv::INTER_LINEAR);
        cv::cvtColor(s, small_, cv::COLOR_BGR2GRAY);
    }
    for (auto& b : boxes) {
        bool known = false;
        for (auto& l : learned_) if (iou(l, b) > 0.3f) { known = true; break; }
        if (known) continue;
        Cand* c = nullptr; float best = cfg_.iou_same;
        for (auto& x : cands_) { float o = iou(x.box, b); if (o >= best) { best = o; c = &x; } }
        if (!c) { cands_.emplace_back(); c = &cands_.back(); c->box = b; }
        if (c->last == fi) continue;
        cv::Mat ring;
        if (c->last < 0 || fi - c->last > cfg_.episode_gap) {
            // 新的一次出现：与上次出现时的背景比较
            ring = ring_patch(small_, b);
            float dd = c->ring_prev.empty() ? -1.f : ring_diff(ring, c->ring_prev);
            if (dd > cfg_.bg_diff_thr) c->changes++;
            if (getenv("PFD_OSD_DEBUG")) fprintf(stderr, "[osd-dbg] f%d cand(%.0f,%.0f,%.0f,%.0f) new-episode diff=%.1f changes=%d hits=%d\n", fi, c->box.x, c->box.y, c->box.width, c->box.height, dd, c->changes, c->hits);
            c->ring_prev = ring; c->ring_ep = ring; c->ep_hits = 0; c->ep_counted = false;
        }
        c->ep_hits++; c->hits++; c->last = fi;
        // 平滑框位置
        c->box = cv::Rect2f(c->box.x * 0.8f + b.x * 0.2f, c->box.y * 0.8f + b.y * 0.2f, c->box.width * 0.8f + b.width * 0.2f, c->box.height * 0.8f + b.height * 0.2f);
        if (!c->ep_counted && c->ep_hits >= cfg_.min_hits) {
            if (ring.empty()) ring = ring_patch(small_, b);
            if (ring_diff(ring, c->ring_ep) > cfg_.bg_diff_thr) { c->changes++; c->ep_counted = true; }
            c->ring_prev = ring;
        }
        if (c->changes >= cfg_.min_changes && c->hits >= cfg_.min_total_hits) {
            learned_.push_back(c->box);
            c->hits = -1000000;   // 标记删除
        }
    }
    cands_.erase(std::remove_if(cands_.begin(), cands_.end(), [&](const Cand& c) { return c.hits < 0 || fi - c.last > cfg_.forget; }), cands_.end());
    if (cands_.size() > 300) cands_.erase(cands_.begin(), cands_.begin() + (cands_.size() - 300));
}

bool OsdFilter::suppressed(const cv::Rect2f& b) const {
    cv::Point2f c(b.x + b.width / 2, b.y + b.height / 2);
    for (auto& u : user_) if (u.contains(c)) return true;
    for (auto& l : learned_) {
        float e = cfg_.mask_expand;
        cv::Rect2f le(l.x - l.width * e, l.y - l.height * e, l.width * (1 + 2 * e), l.height * (1 + 2 * e));
        // 与学到的 OSD 框位置/尺寸接近才屏蔽（真实车牌路过此处时尺寸一般不同）
        if (le.contains(c) && b.width < l.width * 1.6f && b.width > l.width * 0.6f) return true;
    }
    return false;
}

std::vector<cv::Rect2f> parse_regions(const std::string& s, int W, int H) {
    std::vector<cv::Rect2f> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ';')) {
        float v[4]; int n = 0;
        std::stringstream is(item); std::string t;
        while (n < 4 && std::getline(is, t, ',')) v[n++] = std::strtof(t.c_str(), nullptr);
        if (n != 4) continue;
        if (v[0] <= 1.f && v[1] <= 1.f && v[2] <= 1.f && v[3] <= 1.f) out.emplace_back(v[0] * W, v[1] * H, v[2] * W, v[3] * H);
        else out.emplace_back(v[0], v[1], v[2], v[3]);
    }
    return out;
}

}  // namespace pfd
