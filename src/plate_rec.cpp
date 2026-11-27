#include "plate_rec.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <regex>
#include <opencv2/imgproc.hpp>
#include "timer.h"
#include "filters.h"

namespace pfd {
#include "approved_special_format.inc"
namespace crop_policy {
#include "color5_crop_policy.inc"
}
Color5Decision decode_color5_probabilities(const std::vector<Tensor>& outs) {
    Color5Decision r;
    if (outs.size() != 1) { r.status="output_count"; return r; }
    const Tensor& t=outs[0]; size_t n=1;
    if(t.shape.empty()) {r.status="shape";return r;}
    for(int d:t.shape) {if(d<=0 || n>5/(size_t)d){r.status="shape";return r;} n*=d;}
    if(n!=5 || !t.data){r.status="shape_or_null";return r;}
    float sum=0;int best=0;
    for(int k=0;k<5;++k){float p=t.data[k];if(!std::isfinite(p)||p < -1e-6f || p>1.f+1e-6f){r.status="invalid_probability";return r;}sum+=p;if(p>t.data[best])best=k;}
    if(std::abs(sum-1.f)>1e-3f){r.status="probability_sum";return r;}
    static const int map5[5]={PC_BLUE,PC_YELLOW,PC_GREEN,PC_WHITE,PC_BLACK};
    r.color=map5[best];r.conf=std::max(0.f,std::min(1.f,t.data[best]));r.status="ok";return r;
}
bool prepare_color5_input(const cv::Mat& rect, cv::Mat& out) {
    if(rect.empty() || rect.type()!=CV_8UC3){out.release();return false;}
    cv::resize(rect,out,cv::Size(96,96));return true;
}
Color5Decision PlateRecognizer::classify_color5(const cv::Mat& rect) {
    Color5Decision r;
    if(!cls5_){r.status="not_loaded";return r;}
    if(!prepare_color5_input(rect,cls5_in_)){r.status="invalid_roi";return r;}
    std::vector<Tensor> outs;
    if(!cls5_->run(cls5_in_,outs)){r.status="backend_run_failed";return r;}
    return decode_color5_probabilities(outs);
}
int resolve_plate_color5(const std::string& text,int color,int cls_color,int layer,const PlateRecConfig& cfg,const char** src) {
    std::string name=crop_policy::resolve(text,plate_color_cn(color),layer==1?"double":"single");
    // Preserve ORIGINAL production special scopes (not new heuristics), including police-first priority.
    if(text.rfind("WJ",0)==0) name="白";
    else if(text.rfind("粤Z",0)==0 && text.find("警")==std::string::npos) name="黑";
    int c=PC_UNKNOWN;for(int k=0;k<PC_NUM;++k)if(name==plate_color_cn(k)){c=k;break;}
    const char* oldsrc="cls";int result=resolve_plate_color(text,c,cls_color,cfg,&oldsrc);
    if(src)*src=(text.rfind("WJ",0)==0 || text.rfind("粤Z",0)==0 || c!=color || result!=c || text.find("警")!=std::string::npos || text.find("使")!=std::string::npos || text.find("领")!=std::string::npos || text.find("港")!=std::string::npos || text.find("澳")!=std::string::npos || text.find("学")!=std::string::npos || text.find("挂")!=std::string::npos)?"rule5":"color5";
    return result;
}


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
    int approved = approved_special_syntax(ch);
    if (approved >= 0) return approved != 0;
    if (ch.size() < 7 || ch.size() > 8) return false;
    static const std::string prov = "京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼使";
    if (ch[0].size() != 3 || prov.find(ch[0]) == std::string::npos) {
        if (!(ch[0] == "W" && ch[1] == "J")) return false;
    }
    if (ch[1].size() != 1) return false;
    bool ch1_letter = (ch[1][0] >= 'A' && ch[1][0] <= 'Z');
    bool ch1_police_digit = (ch.size() == 7 && ch[6] == "警" && ch[1][0] >= '0' && ch[1][0] <= '9');
    if (!ch1_letter && !ch1_police_digit) return false;
    return true;
}

bool PlateRecognizer::load(const std::string& rec_model, const std::string& cls_model, const std::string& color5_model) {
    cls5_.reset();
    if(!color5_model.empty() && cls_model.empty()) return false; // never replace old selection classifier

    NormConfig rn;  // rec: BGR, (x-127.5)/127.5
    for (int i = 0; i < 3; ++i) { rn.mean[i] = 127.5f; rn.std[i] = 127.5f; }
    rec_ = create_engine();
    if (!rec_->load(rec_model, rn)) return false;
    if (!cls_model.empty()) {
        NormConfig cn;  // cls: BGR /255
        cls_ = create_engine();
        if (!cls_->load(cls_model, cn)) { cls_.reset(); return false; }
    }
    if(!color5_model.empty()) {
        NormConfig cn; // BGR/255 ORT; embedded normalization AX, same established interface
        cls5_=create_engine();
        if(!cls5_->load(color5_model,cn) || cls5_->input_h()!=96 || cls5_->input_w()!=96){cls5_.reset();return false;}
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

static float log_add_exp(float a, float b) {
    const float neg_inf = -std::numeric_limits<float>::infinity();
    if (a == neg_inf) return b;
    if (b == neg_inf) return a;
    float m = std::max(a, b);
    return m + std::log(std::exp(a - m) + std::exp(b - m));
}

// v2.6: CTC Prefix Beam 解码（与锁定 decoder_core.py 口径一致：beam24 / top5，重复字符须经 blank）
static std::pair<std::string, float> decode_ctc_prefix_beam(const float* data, int T, int C, int beam_w, int top_k) {
    const float neg_inf = -std::numeric_limits<float>::infinity();
    const float eps = 1e-12f;
    struct BeamState { float pb = -std::numeric_limits<float>::infinity(); float pnb = -std::numeric_limits<float>::infinity(); };
    std::map<std::vector<int>, BeamState> beams;
    beams[{}] = {0.f, neg_inf};

    std::vector<int> idx(C);
    for (int t = 0; t < T; ++t) {
        const float* p = data + (size_t)t * C;
        // 兼容 logits 或已 softmax 概率：若和偏离 1 则做 softmax
        float sum = 0.f, maxv = p[0];
        for (int c = 0; c < C; ++c) { sum += p[c]; if (p[c] > maxv) maxv = p[c]; }
        std::vector<float> lp(C);
        if (std::abs(sum - 1.f) > 1e-2f || p[0] < 0.f) {
            float esum = 0.f;
            for (int c = 0; c < C; ++c) esum += std::exp(p[c] - maxv);
            float lse = maxv + std::log(std::max(esum, eps));
            for (int c = 0; c < C; ++c) lp[c] = p[c] - lse;
        } else {
            for (int c = 0; c < C; ++c) lp[c] = std::log(std::max(p[c], eps));
        }

        for (int c = 0; c < C; ++c) idx[c] = c;
        int k = std::min(top_k, C);
        std::partial_sort(idx.begin(), idx.begin() + k, idx.end(), [&](int a, int b) { return lp[a] > lp[b]; });
        std::vector<int> cand(idx.begin(), idx.begin() + k);
        if (std::find(cand.begin(), cand.end(), 0) == cand.end()) cand.push_back(0);

        std::map<std::vector<int>, BeamState> next;
        for (const auto& kv : beams) {
            const auto& pref = kv.first;
            float pb = kv.second.pb, pnb = kv.second.pnb;
            float ptot = log_add_exp(pb, pnb);
            for (int c : cand) {
                float lpc = lp[c];
                if (c == 0) {
                    auto& st = next[pref];
                    st.pb = log_add_exp(st.pb, ptot + lpc);
                } else {
                    if (!pref.empty() && pref.back() == c) {
                        // 同字符延续（不经 blank）
                        auto& st_same = next[pref];
                        st_same.pnb = log_add_exp(st_same.pnb, pnb + lpc);
                        // 经 blank 后重复字符
                        std::vector<int> np = pref;
                        np.push_back(c);
                        auto& st_rep = next[np];
                        st_rep.pnb = log_add_exp(st_rep.pnb, pb + lpc);
                    } else {
                        std::vector<int> np = pref;
                        np.push_back(c);
                        auto& st_new = next[np];
                        st_new.pnb = log_add_exp(st_new.pnb, ptot + lpc);
                    }
                }
            }
        }
        // 剪枝保留前 beam_w 条
        std::vector<std::pair<float, std::vector<int>>> scored;
        scored.reserve(next.size());
        for (const auto& kv : next) scored.push_back({log_add_exp(kv.second.pb, kv.second.pnb), kv.first});
        int keep = std::min((int)scored.size(), beam_w);
        std::partial_sort(scored.begin(), scored.begin() + keep, scored.end(),
                          [](const auto& a, const auto& b) { return a.first > b.first; });
        beams.clear();
        for (int i = 0; i < keep; ++i) beams[scored[i].second] = next[scored[i].second];
    }

    if (beams.empty()) return {"", 0.f};
    auto is_digit_tok = [](int idx) { return idx >= 2 && idx <= 11; }; // "0".."9"
    auto is_large_ne_end_tok = [](int idx) {
        if (idx <= 0 || idx >= kNumTokens) return false;
        const char* tok = kTokens[idx];
        return tok[0] != '\0' && tok[1] == '\0' && std::string("ABCDEFGHJK").find(tok[0]) != std::string::npos;
    };
    auto is_valid_first_tok = [](int idx) {
        if (idx <= 0 || idx >= kNumTokens) return false;
        const char* tok = kTokens[idx];
        // 首字符不能是数字(2..11)、单引号(1)、或后缀汉字(学41/挂44/港53/警63/领74/澳76)
        if (idx == 1 || (idx >= 2 && idx <= 11)) return false;
        if (idx == 41 || idx == 44 || idx == 53 || idx == 63 || idx == 74 || idx == 76) return false;
        // 若是汉字(多字节)或 'W' / '使'(75) 则允许
        if ((unsigned char)tok[0] >= 0x80) return true;
        return tok[0] == 'W';
    };
    auto is_prov_tok = [](int idx) {
        if (idx <= 0 || idx >= kNumTokens) return false;
        if (idx == 41 || idx == 44 || idx == 53 || idx == 63 || idx == 74 || idx == 75 || idx == 76) return false;
        return (unsigned char)kTokens[idx][0] >= 0x80;
    };
    std::vector<int> best_pref;
    float best_score = neg_inf;
    float best_raw_score = neg_inf;
    for (const auto& kv : beams) {
        float raw = log_add_exp(kv.second.pb, kv.second.pnb);
        float sc = raw;
        const auto& pf = kv.first;
        // v2.7: 首字符合法性 + 7/8位长度先验 + 禁止双省份汉字 + 8位大型新能源末位约束
        if (!pf.empty() && !is_valid_first_tok(pf[0])) sc -= 6.0f;
        if (pf.size() < 7 || pf.size() > 8) sc -= 4.0f;
        if (pf.size() >= 2 && is_prov_tok(pf[0]) && is_prov_tok(pf[1])) sc -= 6.0f;
        if (pf.size() == 8 && is_digit_tok(pf[2]) && !is_large_ne_end_tok(pf[7])) {
            sc -= 5.0f;
        }
        if (sc > best_score) { best_score = sc; best_raw_score = raw; best_pref = pf; }
    }
    std::string s;
    for (int c : best_pref) if (c > 0 && c < kNumTokens) s += kTokens[c];
    // 置信度：按长度归一化的平均字符概率 exp(raw_score / max(1, len))
    float conf = best_pref.empty() ? 0.f : std::exp(best_raw_score / (float)best_pref.size());
    return {s, std::max(0.f, std::min(1.f, conf))};
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
    if (cfg.prefix_beam) {
        return decode_ctc_prefix_beam(t.data, T, C, cfg.prefix_beam_width, cfg.prefix_top_k);
    }
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
        if (cls_->run(cls_in_, outs) && !outs.empty() && outs[0].size() == 3) {
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
        float rect_ar = (float)r.rectified.cols / std::max(1, r.rectified.rows);
        // v2.7: 检测器判双层且外框呈双层方牌比例(ar<2.4)、双层识别已合法时，不翻回单层
        bool lock_double = (layer == 1 && r.layer == 1 && cur_fs >= 1 && rect_ar < 2.4f);
        bool confident = (cur_fs == 2 && r.conf >= cfg.dual_hyp_conf) || lock_double;
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
    // v2.6: ColorV3-ReLU6 5-class head drives final display/JSON color when loaded (legacy 3-class stays selection/layer authority)
    r.color5_enabled = bool(cls5_);
    if (r.color5_enabled) {
        const char* src5 = "color5";
        r.raw5 = classify_color5(r.rectified);
        r.display_color5 = resolve_plate_color5(r.text, r.raw5.color, r.raw5.color, r.layer, cfg, &src5);
        if (r.display_color5 != PC_UNKNOWN) {
            r.color = r.display_color5;
            r.cls_color = r.raw5.color;
            r.color_conf = r.raw5.conf;
            r.color_src = src5;
        }
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
