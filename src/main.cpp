// plate_face_demo v2: 车牌 + 人脸 全流程离线测试 demo（AX630C / x86 ONNX Runtime 共用）
// v2: OSD 自学习屏蔽、车牌几何/格式/时序过滤、人脸五点几何过滤、BIoU 跟踪、同牌轨迹合并、配置文件
// 读视频 -> 人脸检测(5点) + 车牌检测(4角点+单/双层) -> ByteTrack(各自独立) -> 车牌拉正/识别/颜色
// -> 每个 track 维护 best-shot + 多帧投票 -> 叠加输出视频；可选保存 best-shot 大图/小图 + JSON
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include "bytetrack.h"
#include "detector.h"
#include "filters.h"
#include "plate_rec.h"
#include "text_render.h"
#include "timer.h"

using namespace pfd;

static const std::vector<float> kAnchors = {4, 5, 8, 10, 13, 16, 23, 29, 43, 55, 73, 105, 146, 217, 231, 300, 335, 433};

// ------------------------------------------------------------------ args
struct Args {
    std::string in, out, face, plate, rec, color, snap_dir, dump, osd_mask, result_json;
    // 检测/跟踪阈值（v2 默认值；--legacy 恢复 v1 行为）
    float face_thr = 0.45f, face_new_thr = 0.5f, plate_thr = 0.35f, plate_new_thr = 0.45f, nms = 0.45f;
    float plate_buffer = 0.4f, face_buffer = 0.3f;
    bool fuse_score = false;
    int max_frames = -1, start_frame = 0, track_lost = -1, rec_max = 20, min_hits = 3;
    int plate_display_hits = 2, face_display_hits = 3;
    float plate_show_score = 0.5f;   // 低分轨迹（未达该检测分）须识别出合法车牌后才显示框
    bool double_merge = true, quiet = false;
    std::string double_layer_colors = "yellow";   // v2.1: 允许双层的颜色，逗号分隔；all = 不按颜色修正
    bool layer_fallback = true;
    float layer_fallback_conf = 0.7f;
    // v2.2: ROI 放大二次检测（低分候选附近裁一块放大再检一次，救小/远/模糊车牌）
    int roi_max = 1;              // 每帧最多几个 ROI（0 关闭）
    float roi_seed_thr = 0.12f;   // 候选分数下限（上限为 plate_new_thr）
    float roi_min_w = 60.f;       // 候选框最小宽度（原图像素）
    float roi_zoom = 2.f;
    float rec_count_min_w = 80.f; // v2.2: 小于此宽度(原图像素)且未识别出合法号码的识别不计入 rec_max 预算/非车牌比例（远处小牌先跟上、近了再认）         // 放大倍数：ROI 尺寸 = 原图尺寸 / roi_zoom
    float expand_x = 0.02f, expand_y = 0.04f;
    // 车牌过滤
    bool plate_geom = true, strict_format = true;
    float rec_min_conf = 0.7f, min_vote = 1.0f, valid_ratio = 0.3f;
    int max_serial_letters = 3;
    float merge_gap_s = 4.f;
    // OSD
    bool osd = true, draw_osd = true;
    int osd_episode_gap = 20, osd_min_hits = 25, osd_min_changes = 2;
    float osd_bg_diff = 10.f;
    // 人脸过滤
    bool face_geom = true;
    int face_min_size = 20;
    float face_skin = 0.08f;
    float face_eye_min = 0.06f;     // 两眼距/框宽下限（极端侧脸/俯视两眼重叠时拒绝）        // 肤色比例下限（0 关闭）
    float face_out_score = 0.6f;   // 输出抓拍要求 track 最高分
    bool legacy = false;
};

static void usage(const char* p) {
    printf("用法: %s -i in.mp4 -o out.mp4 --face face.axmodel --plate plate_det.axmodel --rec plate_rec.axmodel\n"
           "          [--color plate_cls.axmodel] [--snap_dir snaps/] [--config demo.cfg]\n"
           "  通用: --max_frames N --start_frame N --track_lost <帧,默认=fps> --min_hits 3 --quiet --dump dets.txt --result_json r.json\n"
           "  车牌检测/跟踪: --plate_thr 0.35(跟踪高分阈值) --plate_new_thr 0.45(新建轨迹) --plate_buffer 0.4(BIoU 外扩) --nms 0.45\n"
           "        --plate_display_hits 2 --plate_show_score 0.5 --rec_max 20 --double_mode merge|separate --expand_x 0.02 --expand_y 0.04\n"
           "        --double_layer_colors yellow(允许双层的颜色, 逗号分隔 blue/yellow/green/white/black, all=不修正) --layer_fallback 1 --layer_fallback_conf 0.7\n"
           "  ROI 二次检测: --roi_max 1(每帧最多 ROI 数, 0 关闭) --roi_seed_thr 0.12 --roi_min_w 60 --roi_zoom 2 --rec_count_min_w 80\n"
           "  车牌过滤: --plate_geom 1(四角点几何) --strict_format 1 --max_serial_letters 3 --rec_min_conf 0.7\n"
           "        --min_vote 1.0 --valid_ratio 0.3(合法识别占比，低于则判非车牌) --merge_gap 4(秒, 同牌轨迹合并)\n"
           "  OSD: --osd 1(自学习屏蔽) --osd_episode_gap 20 --osd_min_hits 25 --osd_min_changes 2 --osd_bg_diff 10 --draw_osd 1\n"
           "        --osd_mask \"x,y,w,h;...\"(用户屏蔽区, <=1 为相对坐标, 例 0.8,0,0.2,0.08)\n"
           "  人脸: --face_thr 0.45 --face_new_thr 0.5 --face_buffer 0.3 --face_eye_min 0.06 --face_geom 1 --face_min_size 20\n"
           "        --face_display_hits 3 --face_out_score 0.6 --face_skin 0.08(框中部肤色比例下限, 0 关闭)\n"
           "  --legacy: 恢复 v1 行为（阈值 0.5/0.55、无过滤、无 OSD、原版 ByteTrack）用于对比\n"
           "  配置文件: 每行 \"key value\"（与命令行同名去掉 --），# 注释；命令行参数优先级更高（写在 --config 之后即可覆盖）\n"
           "  不传 --face 或 --plate 则跳过对应分支；x86 版传 .onnx，板端版传 .axmodel\n",
           p);
}

static bool parse_kv(const std::string& k0, const std::function<std::string()>& next, Args& a) {
    std::string k = k0;
    if (k == "-i" || k == "--input" || k == "input") { a.in = next(); return true; }
    if (k == "-o" || k == "--output" || k == "output") { a.out = next(); return true; }
    if (k.rfind("--", 0) == 0) k = k.substr(2);
    auto F = [&](float& v) { v = std::stof(next()); };
    auto I = [&](int& v) { v = std::stoi(next()); };
    auto B = [&](bool& v) { std::string s = next(); v = !(s == "0" || s == "false" || s == "off"); };
    if (k == "face") a.face = next();
    else if (k == "plate") a.plate = next();
    else if (k == "rec") a.rec = next();
    else if (k == "color") a.color = next();
    else if (k == "snap_dir") a.snap_dir = next();
    else if (k == "dump") a.dump = next();
    else if (k == "result_json") a.result_json = next();
    else if (k == "osd_mask") a.osd_mask = next();
    else if (k == "face_thr") F(a.face_thr);
    else if (k == "face_new_thr") F(a.face_new_thr);
    else if (k == "plate_thr") F(a.plate_thr);
    else if (k == "plate_new_thr") F(a.plate_new_thr);
    else if (k == "plate_buffer") F(a.plate_buffer);
    else if (k == "face_buffer") F(a.face_buffer);
    else if (k == "fuse_score") B(a.fuse_score);
    else if (k == "nms") F(a.nms);
    else if (k == "max_frames") I(a.max_frames);
    else if (k == "start_frame") I(a.start_frame);
    else if (k == "track_lost") I(a.track_lost);
    else if (k == "rec_max") I(a.rec_max);
    else if (k == "min_hits") I(a.min_hits);
    else if (k == "plate_display_hits") I(a.plate_display_hits);
    else if (k == "face_display_hits") I(a.face_display_hits);
    else if (k == "plate_show_score") F(a.plate_show_score);
    else if (k == "double_mode") a.double_merge = next() != "separate";
    else if (k == "double_layer_colors") a.double_layer_colors = next();
    else if (k == "layer_fallback") B(a.layer_fallback);
    else if (k == "layer_fallback_conf") F(a.layer_fallback_conf);
    else if (k == "roi_max") I(a.roi_max);
    else if (k == "roi_seed_thr") F(a.roi_seed_thr);
    else if (k == "roi_min_w") F(a.roi_min_w);
    else if (k == "roi_zoom") F(a.roi_zoom);
    else if (k == "rec_count_min_w") F(a.rec_count_min_w);
    else if (k == "expand_x") F(a.expand_x);
    else if (k == "expand_y") F(a.expand_y);
    else if (k == "plate_geom") B(a.plate_geom);
    else if (k == "strict_format") B(a.strict_format);
    else if (k == "max_serial_letters") I(a.max_serial_letters);
    else if (k == "rec_min_conf") F(a.rec_min_conf);
    else if (k == "min_vote") F(a.min_vote);
    else if (k == "valid_ratio") F(a.valid_ratio);
    else if (k == "merge_gap") F(a.merge_gap_s);
    else if (k == "osd") B(a.osd);
    else if (k == "draw_osd") B(a.draw_osd);
    else if (k == "osd_episode_gap") I(a.osd_episode_gap);
    else if (k == "osd_min_changes") I(a.osd_min_changes);
    else if (k == "osd_min_hits") I(a.osd_min_hits);
    else if (k == "osd_bg_diff") F(a.osd_bg_diff);
    else if (k == "face_geom") B(a.face_geom);
    else if (k == "face_min_size") I(a.face_min_size);
    else if (k == "face_skin") F(a.face_skin);
    else if (k == "face_eye_min") F(a.face_eye_min);
    else if (k == "face_out_score") F(a.face_out_score);
    else if (k == "quiet") a.quiet = true;
    else if (k == "legacy") {
        a.legacy = true;
        a.face_thr = 0.5f; a.face_new_thr = 0.55f; a.plate_thr = 0.5f; a.plate_new_thr = 0.55f;
        a.plate_buffer = 0.f; a.face_buffer = 0.f; a.fuse_score = true; a.plate_display_hits = 1; a.face_display_hits = 1; a.plate_show_score = 0.f;
        a.plate_geom = false; a.strict_format = false; a.rec_min_conf = 0.5f; a.min_vote = 0.f; a.valid_ratio = 0.f;
        a.merge_gap_s = 0.f; a.double_layer_colors = "all"; a.roi_max = 0; a.rec_count_min_w = 0.f; a.osd = false; a.face_geom = false; a.face_skin = 0.f; a.face_min_size = 0; a.face_out_score = 0.f;
    }
    else return false;
    return true;
}

static bool load_config(const std::string& path, Args& a) {
    std::ifstream f(path);
    if (!f) { fprintf(stderr, "无法打开配置文件 %s\n", path.c_str()); return false; }
    std::string line;
    while (std::getline(f, line)) {
        auto h = line.find('#'); if (h != std::string::npos) line = line.substr(0, h);
        std::stringstream ss(line); std::string k, v;
        if (!(ss >> k)) continue;
        std::getline(ss, v);
        v.erase(0, v.find_first_not_of(" \t")); v.erase(v.find_last_not_of(" \t\r") + 1);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        bool used = false;
        if (!parse_kv(k, [&]() { used = true; return v; }, a)) { fprintf(stderr, "配置项未知: %s\n", k.c_str()); return false; }
        (void)used;
    }
    return true;
}

static bool parse(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", k.c_str()); exit(1); } return argv[++i]; };
        if (k == "-h" || k == "--help") { usage(argv[0]); exit(0); }
        if (k == "--config") { if (!load_config(next(), a)) return false; continue; }
        if (!parse_kv(k, next, a)) { fprintf(stderr, "unknown arg %s\n", k.c_str()); usage(argv[0]); return false; }
    }
    if (a.in.empty() || a.out.empty() || (a.face.empty() && a.plate.empty())) { usage(argv[0]); return false; }
    if (!a.plate.empty() && a.rec.empty()) { fprintf(stderr, "--plate 需要同时给 --rec\n"); return false; }
    return true;
}

// ------------------------------------------------------------------ stats
struct Stat { double sum = 0; long n = 0; void add(double v) { sum += v; ++n; } double avg() const { return n ? sum / n : 0; } };
static std::map<std::string, Stat> g_stats;
static std::vector<std::string> g_stat_order = {"decode", "face_pre", "face_infer", "face_post", "plate_pre", "plate_infer",
                                                "plate_post", "plate_roi", "plate_rec", "plate_color", "rec_per_plate", "color_per_plate", "track", "bestshot", "draw", "encode", "algo_total", "frame_total"};

// ------------------------------------------------------------------ quality
static float sharpness(const cv::Mat& bgr, cv::Size norm) {
    if (bgr.empty()) return 0.f;
    cv::Mat g, r, lap;
    cv::cvtColor(bgr, g, cv::COLOR_BGR2GRAY);
    cv::resize(g, r, norm);
    cv::Laplacian(r, lap, CV_32F);
    cv::Scalar m, s;
    cv::meanStdDev(lap, m, s);
    return (float)(s[0] * s[0]);
}

static cv::Rect clip_rect(const cv::Rect2f& r, const cv::Size& s) {
    cv::Rect ri((int)std::floor(r.x), (int)std::floor(r.y), (int)std::ceil(r.width), (int)std::ceil(r.height));
    return ri & cv::Rect(0, 0, s.width, s.height);
}

static cv::Rect2f expand(const cv::Rect2f& r, float s) {
    float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
    return cv::Rect2f(cx - r.width * s / 2, cy - r.height * s / 2, r.width * s, r.height * s);
}

// 人脸质量：置信度 + 尺寸 + 清晰度 + 正脸程度（鼻尖相对两眼中点的偏移）
static float face_quality(const Detection& d, const cv::Mat& frame, float* sharp_out) {
    cv::Rect r = clip_rect(d.box, frame.size());
    float sh = r.area() > 16 ? sharpness(frame(r), {64, 64}) : 0.f;
    if (sharp_out) *sharp_out = sh;
    float size_t_ = std::min(1.f, std::sqrt(d.box.area()) / 96.f);
    float sharp_t = std::min(1.f, sh / 400.f);
    float frontal = 0.5f;
    if (d.kps.size() == 5) {
        cv::Point2f le = d.kps[0], re = d.kps[1], nose = d.kps[2];
        float ed = std::max(1.f, (float)cv::norm(re - le));
        float off = std::fabs(nose.x - (le.x + re.x) / 2) / ed;
        frontal = std::max(0.f, 1.f - 2.f * off);
    }
    return 0.40f * d.score + 0.25f * size_t_ + 0.20f * sharp_t + 0.15f * frontal;
}

static float plate_quality(const Detection& d, const cv::Mat& frame, float* sharp_out) {
    cv::Rect r = clip_rect(d.box, frame.size());
    float sh = r.area() > 16 ? sharpness(frame(r), {96, 32}) : 0.f;
    if (sharp_out) *sharp_out = sh;
    float size_t_ = std::min(1.f, d.box.width / 140.f);   // 车牌宽 >=140px 视为足够
    float sharp_t = std::min(1.f, sh / 800.f);
    return 0.5f * d.score + 0.3f * size_t_ + 0.2f * sharp_t;
}

// ------------------------------------------------------------------ track infos
struct BestShot {
    float quality = -1.f;
    int frame = -1;
    Detection det;
    cv::Mat full;      // 仅在开启 --snap_dir 时保存
    cv::Mat crop;
};

struct FaceInfo {
    int id = 0, first = -1, last = -1, hits = 0;
    float max_score = 0.f;
    BestShot best;
};

struct PlateInfo {
    int id = 0, first = -1, last = -1, hits = 0, rec_count = 0;
    int rec_valid = 0;                       // v2: 识别结果为合法格式（且置信度达标）的次数
    int disp_id = 0;                         // v2: 显示/输出用 ID（同牌短时断开的轨迹复用旧 ID）
    int osd_hits = 0;                        // v2: 落在 OSD 屏蔽区的次数
    float max_score = 0.f;
    BestShot best;
    std::map<std::string, float> votes;      // 车牌号 -> 累计权重
    float color_votes[PC_NUM] = {0};
    float layer_votes[2] = {0};              // 检测器 cls 投票
    float eff_layer_votes[2] = {0};          // v2.1: 识别时实际采用的层数（按颜色修正后）
    int free_recs = 0;                       // v2.2: 不计预算的小框识别次数
    int layer_overridden = 0;                // v2.1: 检测器判双层、被颜色改成单层的识别次数
    int det_layer() const { return layer_votes[1] > layer_votes[0] ? 1 : 0; }
    int disp_layer() const { return (eff_layer_votes[0] + eff_layer_votes[1] > 0) && eff_layer_votes[1] > eff_layer_votes[0] ? 1 : 0; }  // 未识别前不显示双层
    int layer() const { return (eff_layer_votes[0] + eff_layer_votes[1] > 0) ? (eff_layer_votes[1] > eff_layer_votes[0] ? 1 : 0) : det_layer(); }
    std::string raw_last;                    // 最近一次识别原始结果（无合法结果时显示）
    float best_rec_conf = 0.f;
    cv::Mat best_rect;                       // 识别置信度最高的那次的拉正小图
    std::string text() const {
        std::string t; float b = 0;
        for (auto& kv : votes) if (kv.second > b) { b = kv.second; t = kv.first; }
        return t.empty() ? raw_last : t;
    }
    bool valid() const { return !votes.empty(); }
    float text_weight(float* second = nullptr) const {
        float b = 0, s = 0;
        for (auto& kv : votes) { if (kv.second > b) { s = b; b = kv.second; } else if (kv.second > s) s = kv.second; }
        if (second) *second = s;
        return b;
    }
    int color() const {
        int c = PC_UNKNOWN; float b = 0;
        for (int i = 0; i < PC_NUM; ++i) if (color_votes[i] > b) { b = color_votes[i]; c = i; }
        return c;
    }
    bool stable() const { float s; float b = text_weight(&s); return b >= 4.f && b >= 2.5f * s; }
    float valid_ratio() const { return rec_count ? (float)rec_valid / rec_count : 0.f; }
    // 识别多次仍几乎没有合法结果 -> 判为非车牌（广告牌/标语/OSD 等）
    bool non_plate(float min_ratio) const { return min_ratio > 0 && rec_count >= 6 && valid_ratio() < min_ratio; }
};

static std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
    return o;
}

static void mkdir_p(const std::string& p) {
    std::string cur;
    for (size_t i = 0; i < p.size(); ++i) {
        cur += p[i];
        if (p[i] == '/' || i + 1 == p.size()) mkdir(cur.c_str(), 0755);
    }
}

static void save_face_snap(const std::string& dir, const FaceInfo& f, double fps) {
    if (f.best.frame < 0) return;
    char base[512];
    snprintf(base, sizeof(base), "%s/face_%04d", dir.c_str(), f.id);
    if (!f.best.full.empty()) cv::imwrite(std::string(base) + "_full.jpg", f.best.full);
    if (!f.best.crop.empty()) cv::imwrite(std::string(base) + "_crop.jpg", f.best.crop);
    std::ofstream js(std::string(base) + ".json");
    const auto& d = f.best.det;
    js << "{\n  \"type\": \"face\",\n  \"track_id\": " << f.id << ",\n  \"first_frame\": " << f.first
       << ",\n  \"last_frame\": " << f.last << ",\n  \"hits\": " << f.hits << ",\n  \"best_frame\": " << f.best.frame
       << ",\n  \"best_time_s\": " << (fps > 0 ? f.best.frame / fps : 0) << ",\n  \"quality\": " << f.best.quality
       << ",\n  \"score\": " << d.score << ",\n  \"max_score\": " << f.max_score << ",\n  \"box_xywh\": [" << d.box.x << ", " << d.box.y << ", " << d.box.width << ", "
       << d.box.height << "],\n  \"landmarks\": [";
    for (size_t i = 0; i < d.kps.size(); ++i) js << (i ? ", " : "") << "[" << d.kps[i].x << ", " << d.kps[i].y << "]";
    js << "]\n}\n";
}

static void save_plate_snap(const std::string& dir, const PlateInfo& p, double fps) {
    if (p.best.frame < 0) return;
    char base[512];
    snprintf(base, sizeof(base), "%s/plate_%04d", dir.c_str(), p.id);
    if (!p.best.full.empty()) cv::imwrite(std::string(base) + "_full.jpg", p.best.full);
    if (!p.best.crop.empty()) cv::imwrite(std::string(base) + "_crop.jpg", p.best.crop);
    if (!p.best_rect.empty()) cv::imwrite(std::string(base) + "_rect.jpg", p.best_rect);
    std::ofstream js(std::string(base) + ".json");
    const auto& d = p.best.det;
    float second; float w = p.text_weight(&second);
    js << "{\n  \"type\": \"plate\",\n  \"track_id\": " << p.id << ",\n  \"disp_id\": " << p.disp_id
       << ",\n  \"valid_ratio\": " << p.valid_ratio() << ",\n  \"first_frame\": " << p.first
       << ",\n  \"last_frame\": " << p.last << ",\n  \"hits\": " << p.hits << ",\n  \"best_frame\": " << p.best.frame
       << ",\n  \"best_time_s\": " << (fps > 0 ? p.best.frame / fps : 0) << ",\n  \"quality\": " << p.best.quality
       << ",\n  \"det_score\": " << d.score << ",\n  \"det_layer\": \"" << (p.det_layer() ? "double" : "single") << "\",\n  \"layer\": \"" << (p.layer() ? "double" : "single")
       << "\",\n  \"plate_text\": \"" << json_escape(p.text()) << "\",\n  \"color\": \"" << plate_color_en(p.color())
       << "\",\n  \"color_cn\": \"" << plate_color_cn(p.color()) << "\",\n  \"vote_weight\": " << w
       << ",\n  \"vote_second\": " << second << ",\n  \"rec_count\": " << p.rec_count << ",\n  \"best_rec_conf\": " << p.best_rec_conf
       << ",\n  \"box_xywh\": [" << d.box.x << ", " << d.box.y << ", " << d.box.width << ", " << d.box.height << "],\n  \"corners\": [";
    for (size_t i = 0; i < d.kps.size(); ++i) js << (i ? ", " : "") << "[" << d.kps[i].x << ", " << d.kps[i].y << "]";
    js << "],\n  \"votes\": {";
    bool first = true;
    for (auto& kv : p.votes) { js << (first ? "" : ", ") << "\"" << json_escape(kv.first) << "\": " << kv.second; first = false; }
    js << "}\n}\n";
}

// ------------------------------------------------------------------ drawing
static cv::Scalar color_bgr(int c) {
    switch (c) {
        case PC_BLUE: return {200, 80, 0};
        case PC_YELLOW: return {0, 200, 255};
        case PC_GREEN: return {60, 200, 60};
        case PC_WHITE: return {235, 235, 235};
        case PC_BLACK: return {30, 30, 30};
        default: return {128, 128, 128};
    }
}

// ------------------------------------------------------------------ v2: 同牌轨迹合并
struct PlateResult {
    int id = 0, disp_id = 0, first = 0, last = 0, best_frame = 0, color = PC_UNKNOWN, layer = 0;
    std::string text;
    float weight = 0.f;
    std::map<std::string, float> votes;
    std::vector<int> ids;
};

// 时间上相邻（gap 帧内）且车牌号编辑距离 <=1 的结果合并为同一辆车
static std::vector<PlateResult> merge_plates(std::vector<PlateResult> in, int gap) {
    std::sort(in.begin(), in.end(), [](const PlateResult& a, const PlateResult& b) { return a.first < b.first; });
    std::vector<PlateResult> out;
    for (auto& r : in) {
        PlateResult* g = nullptr;
        if (gap > 0)
            for (auto it = out.rbegin(); it != out.rend(); ++it) {
                if (r.first - it->last > gap) continue;
                if (utf8_edit_distance(it->text, r.text) <= 1) { g = &*it; break; }
            }
        if (!g) { out.push_back(r); out.back().ids = {r.id}; continue; }
        for (auto& kv : r.votes) g->votes[kv.first] += kv.second;
        g->weight += r.weight;
        g->first = std::min(g->first, r.first); g->last = std::max(g->last, r.last);
        g->ids.push_back(r.id);
        float b = 0; for (auto& kv : g->votes) if (kv.second > b) { b = kv.second; g->text = kv.first; }
        if (r.weight > 0 && g->color == PC_UNKNOWN) g->color = r.color;
    }
    return out;
}

int main(int argc, char** argv) {
    Args a;
    if (!parse(argc, argv, a)) return 1;
    if (!backend_global_init()) return 2;
    int rc = 0;
    {
        // ---------- models ----------
        std::unique_ptr<YoloKpDetector> face_det, plate_det;
        PlateRecognizer plate_rec;
        NormConfig det_norm; det_norm.to_rgb = true;  // RGB, /255
        if (!a.face.empty()) {
            face_det.reset(new YoloKpDetector(5, 1, kAnchors));
            face_det->pad_value = 114;
            if (!face_det->load(a.face, det_norm)) { fprintf(stderr, "load face model failed\n"); return 3; }
        }
        if (!a.plate.empty()) {
            plate_det.reset(new YoloKpDetector(4, 2, kAnchors));
            plate_det->pad_value = 0;
            if (!plate_det->load(a.plate, det_norm)) { fprintf(stderr, "load plate det model failed\n"); return 3; }
            if (!plate_rec.load(a.rec, a.color)) { fprintf(stderr, "load plate rec/color model failed\n"); return 3; }
            plate_rec.cfg.double_merge = a.double_merge;
            plate_rec.cfg.layer_fallback = a.layer_fallback;
            plate_rec.cfg.layer_fallback_conf = a.layer_fallback_conf;
            plate_rec.cfg.double_colors.clear();
            if (a.double_layer_colors == "all" || a.double_layer_colors == "any") plate_rec.cfg.layer_by_color = false;
            else {
                std::stringstream ss(a.double_layer_colors); std::string c;
                while (std::getline(ss, c, ',')) {
                    bool f = false;
                    for (int i = 0; i < PC_NUM; ++i) if (c == plate_color_en(i) || c == plate_color_cn(i)) { plate_rec.cfg.double_colors.push_back(i); f = true; }
                    if (!f && !c.empty()) { fprintf(stderr, "unknown color in double_layer_colors: %s\n", c.c_str()); return 1; }
                }
            }
            plate_rec.cfg.expand_x = a.expand_x;
            plate_rec.cfg.expand_y = a.expand_y;
        }

        // ---------- video ----------
        cv::VideoCapture cap(a.in);
        if (!cap.isOpened()) {
            fprintf(stderr, "打开输入视频失败: %s（若板端 OpenCV 无 FFmpeg，请先转成 MJPG AVI: "
                            "ffmpeg -i in.mp4 -c:v mjpeg -q:v 3 in.avi）\n", a.in.c_str());
            return 4;
        }
        double fps = cap.get(cv::CAP_PROP_FPS);
        if (!(fps > 1 && fps < 241)) fps = 25;
        int W = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH), H = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        int total = (int)cap.get(cv::CAP_PROP_FRAME_COUNT);
        if (a.start_frame > 0) cap.set(cv::CAP_PROP_POS_FRAMES, a.start_frame);
        std::string ext = a.out.size() > 4 ? a.out.substr(a.out.size() - 4) : "";
        for (auto& c : ext) c = (char)tolower(c);
        int fourcc = ext == ".avi" ? cv::VideoWriter::fourcc('M', 'J', 'P', 'G') : cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        cv::VideoWriter writer;
        if (a.out != "none") {
            writer.open(a.out, fourcc, fps, cv::Size(W, H));
            if (!writer.isOpened()) {
                std::string alt = a.out + ".avi";
                fprintf(stderr, "打开输出 %s 失败，回退为 MJPG AVI: %s\n", a.out.c_str(), alt.c_str());
                writer.open(alt, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, cv::Size(W, H));
                if (!writer.isOpened()) { fprintf(stderr, "输出视频无法创建\n"); return 5; }
            }
        }
        if (!a.snap_dir.empty()) mkdir_p(a.snap_dir);
        FILE* dump = a.dump.empty() ? nullptr : fopen(a.dump.c_str(), "w");
        printf("input %s: %dx%d @ %.2f fps, %d frames%s\n", a.in.c_str(), W, H, fps, total, a.legacy ? "  [LEGACY v1 mode]" : "");
        if (face_det) printf("face model : %s (%s, %dx%d)\n", a.face.c_str(), face_det->backend().c_str(), face_det->input_w(), face_det->input_h());
        if (plate_det) printf("plate model: %s (%s, %dx%d), rec: %s, color: %s\n", a.plate.c_str(), plate_det->backend().c_str(),
                              plate_det->input_w(), plate_det->input_h(), a.rec.c_str(), a.color.empty() ? "(HSV 规则)" : a.color.c_str());

        ByteTrackConfig tcfg;
        tcfg.max_lost = a.track_lost > 0 ? a.track_lost : (int)std::round(fps);
        tcfg.fuse_score = a.fuse_score;
        ByteTrackConfig face_cfg = tcfg, plate_cfg = tcfg;
        face_cfg.track_thresh = a.face_thr; face_cfg.new_track_thresh = a.face_new_thr; face_cfg.buffer = a.face_buffer;
        plate_cfg.track_thresh = a.plate_thr; plate_cfg.new_track_thresh = a.plate_new_thr; plate_cfg.buffer = a.plate_buffer;
        if (a.legacy) {   // v1: 阈值 = max(thr, 0.3)，new = +0.05，fuse_score 开
            face_cfg.track_thresh = std::max(a.face_thr, 0.3f); face_cfg.new_track_thresh = face_cfg.track_thresh + 0.05f;
            plate_cfg.track_thresh = std::max(a.plate_thr, 0.3f); plate_cfg.new_track_thresh = plate_cfg.track_thresh + 0.05f;
        }
        ByteTracker face_trk(face_cfg), plate_trk(plate_cfg);
        PlateGeomConfig pgeo;
        FaceGeomConfig fgeo; fgeo.min_size = a.face_min_size; fgeo.eye_dist_min = a.face_eye_min;
        OsdConfig ocfg; ocfg.enable = a.osd; ocfg.episode_gap = a.osd_episode_gap; ocfg.min_changes = a.osd_min_changes; ocfg.min_hits = a.osd_min_hits; ocfg.bg_diff_thr = a.osd_bg_diff;
        OsdFilter osd(ocfg);
        for (auto& r : parse_regions(a.osd_mask, W, H)) osd.add_user_region(r);
        size_t osd_learned_reported = 0;

        std::map<int, FaceInfo> faces;
        std::map<int, PlateInfo> plates;
        int face_saved = 0, plate_saved = 0;
        std::vector<std::string> plate_summary;
        std::vector<PlateResult> plate_results;
        std::map<std::string, long> cnt;   // 过滤统计
        const int merge_gap = (int)std::round(a.merge_gap_s * fps);
        int next_disp_id = 1;

        auto finish_face = [&](int id) {
            auto it = faces.find(id);
            if (it == faces.end()) return;
            const auto& f = it->second;
            if (f.hits >= std::max(a.min_hits, a.face_display_hits) && f.max_score >= a.face_out_score) {
                cnt["face_tracks_out"]++;
                if (!a.snap_dir.empty()) { save_face_snap(a.snap_dir, f, fps); ++face_saved; }
            } else cnt["face_tracks_dropped"]++;
            faces.erase(it);
        };
        auto plate_ok = [&](const PlateInfo& p) {
            return p.hits >= a.min_hits && p.valid() && p.text_weight() >= a.min_vote && !p.non_plate(a.valid_ratio) &&
                   !(a.osd && p.osd_hits * 2 > p.hits);
        };
        auto finish_plate = [&](int id) {
            auto it = plates.find(id);
            if (it == plates.end()) return;
            auto& p = it->second;
            if (plate_ok(p)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "plate#%d(id %d) %s %s %s frames[%d-%d] best=%d votes=%.2f valid=%.2f det_layer=%s override=%d", p.id, p.disp_id, p.text().c_str(),
                         plate_color_cn(p.color()), p.layer() ? "双" : "单", p.first, p.last, p.best.frame, p.text_weight(), p.valid_ratio(),
                         p.det_layer() ? "双" : "单", p.layer_overridden);
                plate_summary.push_back(buf);
                PlateResult r;
                r.id = p.id; r.disp_id = p.disp_id; r.first = p.first; r.last = p.last; r.best_frame = p.best.frame;
                r.color = p.color(); r.layer = p.layer(); r.text = p.text(); r.weight = p.text_weight(); r.votes = p.votes;
                plate_results.push_back(r);
                if (!a.snap_dir.empty()) { save_plate_snap(a.snap_dir, p, fps); ++plate_saved; }
            } else if (p.hits >= a.min_hits) {
                if (p.non_plate(a.valid_ratio)) cnt["plate_tracks_nonplate"]++;
                else if (a.osd && p.osd_hits * 2 > p.hits) cnt["plate_tracks_osd"]++;
                else cnt["plate_tracks_lowvote"]++;
            }
            plates.erase(it);
        };
        // 新轨迹出现稳定车牌号时，若与最近 merge_gap 帧内结束的轨迹同牌（编辑距离<=1），复用其显示 ID
        auto assign_disp_id = [&](PlateInfo& p, int fidx) {
            if (p.disp_id > 0 && !p.text().empty()) return;
            if (!p.valid() || p.text_weight() < a.min_vote) return;
            std::string t = p.text();
            for (auto itr = plate_results.rbegin(); itr != plate_results.rend(); ++itr) {
                if (fidx - itr->last > merge_gap) break;
                if (utf8_edit_distance(itr->text, t) <= 1) { p.disp_id = itr->disp_id; return; }
            }
            if (p.disp_id == 0) p.disp_id = next_disp_id++;
        };

        cv::Mat frame;
        int fidx = a.start_frame - 1;
        int processed = 0;
        std::vector<double> win;   // 最近 30 帧算法耗时，用于叠加 FPS
        std::map<std::string, double> last;  // 上一帧各模块耗时，用于叠加
        Timer wall;
        while (true) {
            Timer ft;
            if (!cap.read(frame) || frame.empty()) break;
            ++fidx;
            if (a.max_frames > 0 && processed >= a.max_frames) break;
            ++processed;
            std::map<std::string, double> t;
            t["decode"] = ft.lap();
            Timer algo;
            // ---------------- face ----------------
            std::vector<Detection> fd;
            std::vector<float> fgeo_score;
            std::vector<STrack*> ftracks;
            if (face_det) {
                auto raw = face_det->detect(frame, 0.1f /*低分也保留给 ByteTrack 第二阶段*/, a.nms, &t["face_pre"], &t["face_infer"], &t["face_post"]);
                for (auto& d : raw) {
                    float g = 0.5f;
                    if (a.face_geom) {
                        std::string why;
                        g = face_geometry_score(d, fgeo, &why);
                        if (g < 0) { if (d.score >= a.face_thr) cnt["face_det_geom_reject_" + why]++; continue; }
                    } else if (a.face_min_size > 0 && std::min(d.box.width, d.box.height) < a.face_min_size) continue;
                    if (a.face_skin > 0 && d.score >= 0.2f && face_skin_ratio(frame, d.box) < a.face_skin) { if (d.score >= a.face_thr) cnt["face_det_skin_reject"]++; continue; }
                    fd.push_back(d); fgeo_score.push_back(g);
                }
            }
            // ---------------- plate det ----------------
            std::vector<Detection> pd;
            std::vector<char> pd_osd;
            if (plate_det) {
                auto raw = plate_det->detect(frame, 0.1f, a.nms, &t["plate_pre"], &t["plate_infer"], &t["plate_post"]);
                // v2.2: ROI 放大二次检测。全图检测把 2592 宽缩到 640（约 1/4），部分车牌（尤其侧向、反光、对比度低）分数只有 0.1~0.4，
                // 对这类低分候选，在其周围裁 (W/zoom x H/zoom) 的区域再检一次（等效输入分辨率 x zoom），结果与全图检测合并后统一 NMS。
                if (a.roi_max > 0 && a.roi_zoom > 1.f) {
                    Timer rt;
                    std::vector<const Detection*> seeds;
                    for (auto& d : raw) {
                        if (d.score < a.roi_seed_thr || d.score >= a.plate_new_thr || d.box.width < a.roi_min_w) continue;
                        if (osd.suppressed(d.box)) continue;
                        bool covered = false;
                        for (auto& h : raw) if (h.score >= a.plate_new_thr && iou(h.box, d.box) > 0.3f) { covered = true; break; }
                        if (!covered) seeds.push_back(&d);
                    }
                    std::sort(seeds.begin(), seeds.end(), [](const Detection* x, const Detection* y) { return x->score > y->score; });
                    std::vector<cv::Rect> rois;
                    int rw = std::min(W, (int)std::round(W / a.roi_zoom)), rh = std::min(H, (int)std::round(H / a.roi_zoom));
                    for (auto* sd : seeds) {
                        if ((int)rois.size() >= a.roi_max) break;
                        cv::Point2f c(sd->box.x + sd->box.width / 2, sd->box.y + sd->box.height / 2);
                        bool inside = false;
                        for (auto& r : rois) if (r.contains(cv::Point((int)c.x, (int)c.y))) { inside = true; break; }
                        if (inside) continue;
                        int x0 = std::max(0, std::min(W - rw, (int)std::round(c.x - rw / 2.f)));
                        int y0 = std::max(0, std::min(H - rh, (int)std::round(c.y - rh / 2.f)));
                        rois.emplace_back(x0, y0, rw, rh);
                    }
                    for (auto& r : rois) {
                        auto rd = plate_det->detect(frame(r), 0.1f, a.nms);
                        cnt["plate_roi_runs"]++;
                        const float m = 3.f;
                        for (auto& d : rd) {
                            // 被 ROI 边界截断的框丢弃（ROI 边界与原图边界重合时除外）
                            if ((d.box.x < m && r.x > 0) || (d.box.y < m && r.y > 0) ||
                                (d.box.x + d.box.width > r.width - m && r.x + r.width < W) || (d.box.y + d.box.height > r.height - m && r.y + r.height < H)) continue;
                            d.box.x += r.x; d.box.y += r.y;
                            for (auto& k : d.kps) { k.x += r.x; k.y += r.y; }
                            raw.push_back(d);
                        }
                    }
                    if (!rois.empty()) nms(raw, a.nms);   // 类别无关 NMS，ROI 的高分框替换全图低分框
                    t["plate_roi"] = rt.lap();
                }
                for (auto& d : raw) {
                    if (a.plate_geom) {
                        std::string why;
                        if (!plate_geometry_ok(d, pgeo, &why)) { if (d.score >= a.plate_thr) cnt["plate_det_geom_reject_" + why]++; continue; }
                    }
                    pd.push_back(d);
                }
                for (auto& d : pd) pd_osd.push_back(osd.suppressed(d.box) ? 1 : 0);
                // OSD 屏蔽区内的检测直接丢弃（不参与跟踪）
                std::vector<Detection> keep;
                for (size_t i = 0; i < pd.size(); ++i) { if (pd_osd[i]) { if (pd[i].score >= a.plate_thr) cnt["plate_det_osd_suppressed"]++; } else keep.push_back(pd[i]); }
                pd.swap(keep);
            }
            ft.lap();
            // ---------------- tracking ----------------
            std::vector<STrack*> ptracks;
            {
                std::vector<cv::Rect2f> b; std::vector<float> s;
                if (face_det) {
                    for (auto& d : fd) { b.push_back(d.box); s.push_back(d.score); }
                    ftracks = face_trk.update(b, s);
                    for (auto& r : face_trk.removed_this_frame()) finish_face(r.id);
                }
                if (plate_det) {
                    b.clear(); s.clear();
                    for (auto& d : pd) { b.push_back(d.box); s.push_back(d.score); }
                    ptracks = plate_trk.update(b, s);
                    for (auto& r : plate_trk.removed_this_frame()) finish_plate(r.id);
                }
            }
            t["track"] = ft.lap();
            // ---------------- best-shot (face) ----------------
            for (auto* tr : ftracks) {
                if (tr->det_idx < 0) continue;
                auto& fi = faces[tr->id];
                if (fi.first < 0) { fi.id = tr->id; fi.first = fidx; }
                fi.last = fidx; fi.hits++;
                const Detection& d = fd[tr->det_idx];
                fi.max_score = std::max(fi.max_score, d.score);
                float q = face_quality(d, frame, nullptr);
                if (a.face_geom) q = 0.8f * q + 0.2f * fgeo_score[tr->det_idx];
                if (q > fi.best.quality) {
                    fi.best.quality = q; fi.best.frame = fidx; fi.best.det = d;
                    if (!a.snap_dir.empty()) {
                        fi.best.full = frame.clone();
                        cv::Rect r = clip_rect(expand(d.box, 1.3f), frame.size());
                        fi.best.crop = r.area() > 0 ? frame(r).clone() : cv::Mat();
                    }
                }
            }
            double t_track_extra = ft.lap();
            // ---------------- plate rec + vote ----------------
            double t_rec = 0, t_col = 0; int n_rec = 0;
            for (auto* tr : ptracks) {
                if (tr->det_idx < 0) continue;
                auto& pi = plates[tr->id];
                if (pi.first < 0) { pi.id = tr->id; pi.first = fidx; }
                pi.last = fidx; pi.hits++;
                const Detection& d = pd[tr->det_idx];
                pi.max_score = std::max(pi.max_score, d.score);
                pi.layer_votes[d.cls == 1 ? 1 : 0] += d.score;
                float q = plate_quality(d, frame, nullptr);
                bool better = q > pi.best.quality;
                if (better) {
                    pi.best.quality = q; pi.best.frame = fidx; pi.best.det = d;
                    if (!a.snap_dir.empty()) {
                        pi.best.full = frame.clone();
                        cv::Rect r = clip_rect(expand(d.box, 1.2f), frame.size());
                        pi.best.crop = r.area() > 0 ? frame(r).clone() : cv::Mat();
                    }
                }
                // 识别策略: 未稳定前每帧识别；稳定后仅在质量提升时识别；单 track 上限 rec_max 次
                bool need = pi.rec_count < a.rec_max && (!pi.stable() || better) && d.box.width >= 24 && d.box.height >= 8;
                if (!need) continue;
                int layer = pi.layer_votes[1] > pi.layer_votes[0] ? 1 : 0;
                double tr_ = 0, tc_ = 0;
                PlateRecResult rr = plate_rec.recognize(frame, d.kps, layer, &tr_, &tc_);
                t_rec += tr_; t_col += tc_; ++n_rec;
                pi.rec_count++;
                pi.eff_layer_votes[rr.layer] += 1.f;
                if (rr.layer_overridden) { pi.layer_overridden++; cnt["plate_rec_layer_overridden"]++; }
                if (rr.det_layer == 1 && rr.layer == 1 && !a.legacy) cnt["plate_rec_double_kept"]++;
                if (rr.text.empty()) {
                    if (d.box.width < a.rec_count_min_w && pi.free_recs < a.rec_max) { pi.free_recs++; pi.rec_count--; }
                    continue;
                }
                pi.raw_last = rr.text;
                bool fmt = a.strict_format ? strict_plate_format(rr.text, a.max_serial_letters) : rr.valid_format;
                float w = rr.conf * (0.5f + 0.5f * q);
                if (!a.legacy && rr.color >= 0) w *= 0.5f + 0.5f * rr.color_conf;   // 颜色分类置信度低（不像车牌底色）降权
                if (fmt && rr.conf > a.rec_min_conf) { pi.votes[rr.text] += w; pi.rec_valid++; }
                if (rr.color >= 0) pi.color_votes[rr.color] += rr.color_conf * w;
                if (fmt && rr.conf > pi.best_rec_conf) { pi.best_rec_conf = rr.conf; pi.best_rect = rr.rectified.clone(); }
                if (!a.legacy) assign_disp_id(pi, fidx);
                if (!(fmt && rr.conf > a.rec_min_conf) && d.box.width < a.rec_count_min_w && pi.free_recs < a.rec_max) { pi.free_recs++; pi.rec_count--; }
            }
            // OSD 学习：只用"非车牌"的检测（所在轨迹没有稳定合法车牌号），屏蔽从下一帧生效
            if (plate_det && a.osd) {
                std::vector<cv::Rect2f> obs;
                for (auto* tr : ptracks) {
                    if (tr->det_idx < 0) continue;
                    auto it = plates.find(tr->id);
                    bool real = it != plates.end() && it->second.valid() && it->second.text_weight() >= a.min_vote && it->second.valid_ratio() >= 0.5f;
                    if (!real && pd[tr->det_idx].score >= a.plate_thr) obs.push_back(pd[tr->det_idx].box);
                }
                osd.update(fidx, frame, obs);
                while (osd_learned_reported < osd.learned().size()) {
                    auto& b = osd.learned()[osd_learned_reported++];
                    printf("[osd] frame %d learned OSD mask #%zu at (%.0f,%.0f,%.0f,%.0f)\n", fidx, osd_learned_reported, b.x, b.y, b.width, b.height);
                }
            }
            t["plate_rec"] = t_rec; t["plate_color"] = t_col;
            if (n_rec > 0) { g_stats["rec_per_plate"].add(t_rec / n_rec); g_stats["color_per_plate"].add(t_col / n_rec); }
            t["bestshot"] = t_track_extra + std::max(0.0, ft.lap() - t_rec - t_col);
            double algo_ms = algo.total();
            t["algo_total"] = algo_ms;

            // ---------------- draw ----------------
            Timer dt;
            int n_face_shown = 0, n_plate_shown = 0;
            if (dump) fprintf(dump, "N %d %zu %zu\n", fidx, fd.size(), pd.size());
            if (a.draw_osd) {
                for (auto& r : osd.user()) { cv::rectangle(frame, r, cv::Scalar(160, 160, 160), 1); }
                for (auto& r : osd.learned()) {
                    cv::rectangle(frame, r, cv::Scalar(160, 160, 160), 1);
                    put_text(frame, "OSD", cv::Point((int)r.x, (int)(r.y + r.height + 2)), std::max(12, H / 60), cv::Scalar(200, 200, 200));
                }
            }
            for (auto* tr : ftracks) {
                if (tr->det_idx < 0) continue;
                if (tr->hits < a.face_display_hits) continue;
                const Detection& d = fd[tr->det_idx];
                ++n_face_shown;
                if (dump) fprintf(dump, "SF %d %d %.3f %.0f %.0f %.0f %.0f\n", fidx, tr->id, d.score, d.box.x, d.box.y, d.box.width, d.box.height);
                cv::rectangle(frame, d.box, cv::Scalar(0, 255, 0), 2);
                static const cv::Scalar kc[5] = {{255, 0, 0}, {0, 0, 255}, {0, 255, 255}, {255, 0, 255}, {255, 255, 0}};
                for (size_t k = 0; k < d.kps.size() && k < 5; ++k) cv::circle(frame, d.kps[k], 3, kc[k], -1);
                char buf[64]; snprintf(buf, sizeof(buf), "F%d %.2f", tr->id, d.score);
                cv::Scalar bg(0, 140, 0);
                int th = std::max(16, H / 34);
                put_text(frame, buf, cv::Point((int)d.box.x, std::max(0, (int)d.box.y - th - 4)), th, cv::Scalar(255, 255, 255), &bg);
            }
            for (auto* tr : ptracks) {
                if (tr->det_idx < 0) continue;
                const Detection& d = pd[tr->det_idx];
                auto it = plates.find(tr->id);
                if (tr->hits < a.plate_display_hits) continue;
                if (it != plates.end() && it->second.non_plate(a.valid_ratio)) continue;   // 判为非车牌的不画
                bool ok = it != plates.end() && it->second.valid() && it->second.text_weight() >= a.min_vote;
                if (!ok && (it == plates.end() || it->second.max_score < a.plate_show_score)) continue;   // 低分且未识别出合法车牌：不画
                ++n_plate_shown;
                std::vector<cv::Point> poly;
                for (auto& p : d.kps) poly.emplace_back((int)p.x, (int)p.y);
                cv::rectangle(frame, d.box, cv::Scalar(255, 160, 0), 1);
                cv::polylines(frame, poly, true, cv::Scalar(0, 255, 255), 2);
                std::string txt = it != plates.end() ? (a.legacy ? it->second.text() : (ok ? it->second.text() : "")) : "";
                int col = it != plates.end() ? it->second.color() : PC_UNKNOWN;
                int show_id = (it != plates.end() && it->second.disp_id > 0) ? it->second.disp_id : tr->id;
                if (dump) fprintf(dump, "SP %d %d %d %.3f %.0f %.0f %.0f %.0f %s %c%c\n", fidx, tr->id, show_id, d.score, d.box.x, d.box.y, d.box.width, d.box.height, txt.empty() ? "-" : txt.c_str(),
                                  d.cls == 1 ? 'D' : 'S', (a.legacy ? d.cls : (it != plates.end() ? it->second.disp_layer() : 0)) == 1 ? 'D' : 'S');
                int lay = a.legacy ? d.cls : (it != plates.end() ? it->second.disp_layer() : 0);
                char head[64]; snprintf(head, sizeof(head), "P%d%s ", show_id, lay == 1 ? "(双)" : "");
                std::string label = std::string(head) + (txt.empty() ? "..." : txt) + (ok || txt.empty() ? "" : "?") + " " + (col >= 0 && !txt.empty() ? plate_color_cn(col) : "");
                cv::Scalar bg = txt.empty() ? cv::Scalar(128, 128, 128) : color_bgr(col);
                cv::Scalar fg = (!txt.empty() && (col == PC_WHITE || col == PC_YELLOW)) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);
                int th = std::max(18, H / 28);
                put_text(frame, label, cv::Point((int)d.box.x, std::max(0, (int)d.box.y - th - 6)), th, fg, &bg, 3);
            }
            // 左上角耗时面板（显示上一帧完整耗时 + 本帧算法耗时）
            win.push_back(algo_ms); if (win.size() > 30) win.erase(win.begin());
            double avg = 0; for (double v : win) avg += v; avg /= win.size();
            {
                auto g = [&](const char* k) { auto i = t.find(k); return i == t.end() ? 0.0 : i->second; };
                std::vector<std::string> lines;
                char b[200];
                snprintf(b, sizeof(b), "frame %d  algo %.1f ms  FPS(algo) %.1f  FPS(e2e) %.1f  %s", fidx, algo_ms, 1000.0 / std::max(avg, 1e-3),
                         last.count("frame_total") ? 1000.0 / std::max(last["frame_total"], 1e-3) : 0.0, a.legacy ? "v1" : "v2");
                lines.push_back(b);
                if (face_det) { snprintf(b, sizeof(b), "face det  pre %.1f infer %.1f post %.1f ms  n=%d shown=%d", g("face_pre"), g("face_infer"), g("face_post"), (int)fd.size(), n_face_shown); lines.push_back(b); }
                if (plate_det) {
                    snprintf(b, sizeof(b), "plate det pre %.1f infer %.1f post %.1f roi %.1f ms  n=%d shown=%d osd=%zu", g("plate_pre"), g("plate_infer"), g("plate_post"), g("plate_roi"), (int)pd.size(), n_plate_shown, osd.learned().size() + osd.user().size()); lines.push_back(b);
                    snprintf(b, sizeof(b), "plate rec %.1f ms (x%d)  color %.1f ms", t_rec, n_rec, t_col); lines.push_back(b);
                }
                snprintf(b, sizeof(b), "track %.2f  best-shot %.2f  decode %.1f  draw %.1f  encode %.1f ms  [%s]", g("track"), g("bestshot"), g("decode"),
                         last.count("draw") ? last["draw"] : 0.0, last.count("encode") ? last["encode"] : 0.0,
                         face_det ? face_det->backend().c_str() : plate_det->backend().c_str());
                lines.push_back(b);
                int th = std::max(14, H / 38);
                int pw = 0; for (auto& l : lines) pw = std::max(pw, text_size(l, th).width);
                cv::Rect panel(4, 4, std::min(W - 8, pw + 12), (int)lines.size() * (th + 4) + 8);
                cv::Mat roi = frame(panel);
                cv::Mat dark(roi.size(), roi.type(), cv::Scalar(0, 0, 0));
                cv::addWeighted(roi, 0.45, dark, 0.55, 0, roi);
                for (size_t i = 0; i < lines.size(); ++i)
                    put_text(frame, lines[i], cv::Point(10, 8 + (int)i * (th + 4)), th, cv::Scalar(255, 255, 255));
            }
            t["draw"] = dt.lap();
            if (writer.isOpened()) writer.write(frame);
            t["encode"] = dt.lap();
            t["frame_total"] = ft.total();
            for (auto& kv : t) g_stats[kv.first].add(kv.second);
            last = t;
            if (!a.quiet && (fidx % 30 == 0))
                printf("[frame %d/%d] algo %.1f ms, faces %zu (shown %d), plates %zu (shown %d)\n", fidx, total, algo_ms, fd.size(), n_face_shown, pd.size(), n_plate_shown);
        }
        // 视频结束，输出所有剩余 track
        std::vector<int> ids;
        for (auto& kv : faces) ids.push_back(kv.first);
        for (int id : ids) finish_face(id);
        ids.clear();
        for (auto& kv : plates) ids.push_back(kv.first);
        for (int id : ids) finish_plate(id);
        writer.release();
        if (dump) fclose(dump);

        printf("\n================ summary ================\n");
        printf("frames: %d, wall %.1f s, output: %s\n", processed, wall.total() / 1000.0, a.out.c_str());
        printf("%-16s %10s\n", "module", "avg ms");
        for (auto& k : g_stat_order) {
            auto it = g_stats.find(k);
            if (it == g_stats.end()) continue;
            // plate_rec / plate_color 为按帧平均（含不识别的帧）；rec_per_plate / color_per_plate 为单次识别平均
            printf("%-16s %10.2f  (n=%ld)\n", k.c_str(), it->second.avg(), it->second.n);
        }
        auto at = g_stats.find("algo_total");
        if (at != g_stats.end() && at->second.avg() > 0) printf("avg algo FPS: %.2f\n", 1000.0 / at->second.avg());
        auto ftot = g_stats.find("frame_total");
        if (ftot != g_stats.end() && ftot->second.avg() > 0) printf("avg end-to-end FPS (含解码/叠加/编码): %.2f\n", 1000.0 / ftot->second.avg());
        printf("filter stats:\n");
        for (auto& kv : cnt) printf("  %-36s %ld\n", kv.first.c_str(), kv.second);
        printf("osd masks: learned %zu, user %zu\n", osd.learned().size(), osd.user().size());
        for (auto& b : osd.learned()) printf("  learned (%.0f,%.0f,%.0f,%.0f)\n", b.x, b.y, b.width, b.height);
        printf("plate tracks (hits>=%d, 有合法车牌号): %zu\n", a.min_hits, plate_summary.size());
        for (auto& s : plate_summary) printf("  %s\n", s.c_str());
        auto merged = merge_plates(plate_results, merge_gap);
        printf("plate results merged (同牌、间隔<=%.1fs 合并): %zu\n", a.merge_gap_s, merged.size());
        for (auto& m : merged) {
            std::string idl; for (int i : m.ids) idl += (idl.empty() ? "" : ",") + std::to_string(i);
            printf("  %s %s%s frames[%d-%d] t=%.1fs weight=%.2f tracks=[%s]\n", m.text.c_str(), plate_color_cn(m.color), m.layer ? "(双)" : "", m.first, m.last,
                   m.first / fps, m.weight, idl.c_str());
        }
        if (!a.result_json.empty()) {
            std::ofstream js(a.result_json);
            js << "{\n  \"plates\": [\n";
            for (size_t i = 0; i < merged.size(); ++i) {
                auto& m = merged[i];
                js << "    {\"plate\": \"" << json_escape(m.text) << "\", \"color\": \"" << plate_color_cn(m.color) << "\", \"layer\": \"" << (m.layer ? "double" : "single") << "\", \"first_frame\": " << m.first
                   << ", \"last_frame\": " << m.last << ", \"first_time_s\": " << m.first / fps << ", \"weight\": " << m.weight << ", \"tracks\": [";
                for (size_t k = 0; k < m.ids.size(); ++k) js << (k ? ", " : "") << m.ids[k];
                js << "]}" << (i + 1 < merged.size() ? "," : "") << "\n";
            }
            js << "  ],\n  \"face_tracks\": " << cnt["face_tracks_out"] << "\n}\n";
        }
        printf("face tracks output: %ld (dropped %ld)\n", cnt["face_tracks_out"], cnt["face_tracks_dropped"]);
        if (!a.snap_dir.empty()) printf("snapshots: %d faces, %d plates -> %s\n", face_saved, plate_saved, a.snap_dir.c_str());
    }
    backend_global_deinit();
    return rc;
}
