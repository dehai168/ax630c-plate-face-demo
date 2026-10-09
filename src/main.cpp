// plate_face_demo: 车牌 + 人脸 全流程离线测试 demo（AX630C / x86 ONNX Runtime 共用）
// 读视频 -> 人脸检测(5点) + 车牌检测(4角点+单/双层) -> ByteTrack(各自独立) -> 车牌拉正/识别/颜色
// -> 每个 track 维护 best-shot + 多帧投票 -> 叠加输出视频；可选保存 best-shot 大图/小图 + JSON
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
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
#include "plate_rec.h"
#include "text_render.h"
#include "timer.h"

using namespace pfd;

static const std::vector<float> kAnchors = {4, 5, 8, 10, 13, 16, 23, 29, 43, 55, 73, 105, 146, 217, 231, 300, 335, 433};

// ------------------------------------------------------------------ args
struct Args {
    std::string in, out, face, plate, rec, color, snap_dir;
    float face_thr = 0.5f, plate_thr = 0.5f, nms = 0.45f;
    int max_frames = -1, track_lost = -1, rec_max = 20, min_hits = 3;
    bool double_merge = true, quiet = false;
    float expand_x = 0.02f, expand_y = 0.04f;
};

static void usage(const char* p) {
    printf("用法: %s -i in.mp4 -o out.mp4 --face face.axmodel --plate plate_det.axmodel --rec plate_rec.axmodel\n"
           "          [--color plate_cls.axmodel] [--snap_dir snaps/]\n"
           "  可选: --face_thr 0.5 --plate_thr 0.5 --nms 0.45 --max_frames N --track_lost <帧,默认=fps>\n"
           "        --rec_max 20(每个车牌 track 最多识别次数) --min_hits 3 --double_mode merge|separate\n"
           "        --expand_x 0.02 --expand_y 0.04 (车牌四角点外扩比例) --quiet\n"
           "  不传 --face 或 --plate 则跳过对应分支；x86 版传 .onnx，板端版传 .axmodel\n"
           "  输出扩展名 .avi -> MJPG 编码，其他 -> mp4v(MPEG-4 Part2)\n", p);
}

static bool parse(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", k.c_str()); exit(1); } return argv[++i]; };
        if (k == "-i" || k == "--input") a.in = next();
        else if (k == "-o" || k == "--output") a.out = next();
        else if (k == "--face") a.face = next();
        else if (k == "--plate") a.plate = next();
        else if (k == "--rec") a.rec = next();
        else if (k == "--color") a.color = next();
        else if (k == "--snap_dir") a.snap_dir = next();
        else if (k == "--face_thr") a.face_thr = std::stof(next());
        else if (k == "--plate_thr") a.plate_thr = std::stof(next());
        else if (k == "--nms") a.nms = std::stof(next());
        else if (k == "--max_frames") a.max_frames = std::stoi(next());
        else if (k == "--track_lost") a.track_lost = std::stoi(next());
        else if (k == "--rec_max") a.rec_max = std::stoi(next());
        else if (k == "--min_hits") a.min_hits = std::stoi(next());
        else if (k == "--double_mode") a.double_merge = next() != "separate";
        else if (k == "--expand_x") a.expand_x = std::stof(next());
        else if (k == "--expand_y") a.expand_y = std::stof(next());
        else if (k == "--quiet") a.quiet = true;
        else if (k == "-h" || k == "--help") { usage(argv[0]); exit(0); }
        else { fprintf(stderr, "unknown arg %s\n", k.c_str()); usage(argv[0]); return false; }
    }
    if (a.in.empty() || a.out.empty() || (a.face.empty() && a.plate.empty())) { usage(argv[0]); return false; }
    if (!a.plate.empty() && a.rec.empty()) { fprintf(stderr, "--plate 需要同时给 --rec\n"); return false; }
    return true;
}

// ------------------------------------------------------------------ stats
struct Stat { double sum = 0; long n = 0; void add(double v) { sum += v; ++n; } double avg() const { return n ? sum / n : 0; } };
static std::map<std::string, Stat> g_stats;
static std::vector<std::string> g_stat_order = {"decode", "face_pre", "face_infer", "face_post", "plate_pre", "plate_infer",
                                                "plate_post", "plate_rec", "plate_color", "rec_per_plate", "color_per_plate", "track", "bestshot", "draw", "encode", "algo_total", "frame_total"};

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
    BestShot best;
};

struct PlateInfo {
    int id = 0, first = -1, last = -1, hits = 0, rec_count = 0;
    BestShot best;
    std::map<std::string, float> votes;      // 车牌号 -> 累计权重
    float color_votes[PC_NUM] = {0};
    float layer_votes[2] = {0};
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
       << ",\n  \"score\": " << d.score << ",\n  \"box_xywh\": [" << d.box.x << ", " << d.box.y << ", " << d.box.width << ", "
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
    js << "{\n  \"type\": \"plate\",\n  \"track_id\": " << p.id << ",\n  \"first_frame\": " << p.first
       << ",\n  \"last_frame\": " << p.last << ",\n  \"hits\": " << p.hits << ",\n  \"best_frame\": " << p.best.frame
       << ",\n  \"best_time_s\": " << (fps > 0 ? p.best.frame / fps : 0) << ",\n  \"quality\": " << p.best.quality
       << ",\n  \"det_score\": " << d.score << ",\n  \"layer\": \"" << (p.layer_votes[1] > p.layer_votes[0] ? "double" : "single")
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
        std::string ext = a.out.size() > 4 ? a.out.substr(a.out.size() - 4) : "";
        for (auto& c : ext) c = (char)tolower(c);
        int fourcc = ext == ".avi" ? cv::VideoWriter::fourcc('M', 'J', 'P', 'G') : cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        cv::VideoWriter writer(a.out, fourcc, fps, cv::Size(W, H));
        if (!writer.isOpened()) {
            std::string alt = a.out + ".avi";
            fprintf(stderr, "打开输出 %s 失败，回退为 MJPG AVI: %s\n", a.out.c_str(), alt.c_str());
            writer.open(alt, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, cv::Size(W, H));
            if (!writer.isOpened()) { fprintf(stderr, "输出视频无法创建\n"); return 5; }
        }
        if (!a.snap_dir.empty()) mkdir_p(a.snap_dir);
        printf("input %s: %dx%d @ %.2f fps, %d frames\n", a.in.c_str(), W, H, fps, total);
        if (face_det) printf("face model : %s (%s, %dx%d)\n", a.face.c_str(), face_det->backend().c_str(), face_det->input_w(), face_det->input_h());
        if (plate_det) printf("plate model: %s (%s, %dx%d), rec: %s, color: %s\n", a.plate.c_str(), plate_det->backend().c_str(),
                              plate_det->input_w(), plate_det->input_h(), a.rec.c_str(), a.color.empty() ? "(HSV 规则)" : a.color.c_str());

        ByteTrackConfig tcfg;
        tcfg.max_lost = a.track_lost > 0 ? a.track_lost : (int)std::round(fps);
        ByteTrackConfig face_cfg = tcfg, plate_cfg = tcfg;
        face_cfg.track_thresh = std::max(a.face_thr, 0.3f); face_cfg.new_track_thresh = face_cfg.track_thresh + 0.05f;
        plate_cfg.track_thresh = std::max(a.plate_thr, 0.3f); plate_cfg.new_track_thresh = plate_cfg.track_thresh + 0.05f;
        ByteTracker face_trk(face_cfg), plate_trk(plate_cfg);
        std::map<int, FaceInfo> faces;
        std::map<int, PlateInfo> plates;
        int face_saved = 0, plate_saved = 0;
        std::vector<std::string> plate_summary;

        auto finish_face = [&](int id) {
            auto it = faces.find(id);
            if (it == faces.end()) return;
            if (it->second.hits >= a.min_hits && !a.snap_dir.empty()) { save_face_snap(a.snap_dir, it->second, fps); ++face_saved; }
            faces.erase(it);
        };
        auto finish_plate = [&](int id) {
            auto it = plates.find(id);
            if (it == plates.end()) return;
            auto& p = it->second;
            if (p.hits >= a.min_hits && p.valid()) {
                char buf[256];
                snprintf(buf, sizeof(buf), "plate#%d %s %s frames[%d-%d] best=%d votes=%.2f", p.id, p.text().c_str(),
                         plate_color_cn(p.color()), p.first, p.last, p.best.frame, p.text_weight());
                plate_summary.push_back(buf);
                if (!a.snap_dir.empty()) { save_plate_snap(a.snap_dir, p, fps); ++plate_saved; }
            }
            plates.erase(it);
        };

        cv::Mat frame;
        int fidx = -1;
        std::vector<double> win;   // 最近 30 帧算法耗时，用于叠加 FPS
        std::map<std::string, double> last;  // 上一帧各模块耗时，用于叠加
        Timer wall;
        while (true) {
            Timer ft;
            if (!cap.read(frame) || frame.empty()) break;
            ++fidx;
            if (a.max_frames > 0 && fidx >= a.max_frames) break;
            std::map<std::string, double> t;
            t["decode"] = ft.lap();
            Timer algo;
            // ---------------- face ----------------
            std::vector<Detection> fd;
            std::vector<STrack*> ftracks;
            if (face_det) {
                fd = face_det->detect(frame, 0.1f /*低分也保留给 ByteTrack 第二阶段*/, a.nms, &t["face_pre"], &t["face_infer"], &t["face_post"]);
            }
            // ---------------- plate det ----------------
            std::vector<Detection> pd;
            if (plate_det) {
                pd = plate_det->detect(frame, 0.1f, a.nms, &t["plate_pre"], &t["plate_infer"], &t["plate_post"]);
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
                float q = face_quality(d, frame, nullptr);
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
                if (rr.text.empty()) continue;
                pi.raw_last = rr.text;
                float w = rr.conf * (0.5f + 0.5f * q);
                if (rr.valid_format && rr.conf > 0.5f) pi.votes[rr.text] += w;
                if (rr.color >= 0) pi.color_votes[rr.color] += rr.color_conf * w;
                if (rr.valid_format && rr.conf > pi.best_rec_conf) { pi.best_rec_conf = rr.conf; pi.best_rect = rr.rectified.clone(); }
            }
            t["plate_rec"] = t_rec; t["plate_color"] = t_col;
            if (n_rec > 0) { g_stats["rec_per_plate"].add(t_rec / n_rec); g_stats["color_per_plate"].add(t_col / n_rec); }
            t["bestshot"] = t_track_extra + std::max(0.0, ft.lap() - t_rec - t_col);
            double algo_ms = algo.total();
            t["algo_total"] = algo_ms;

            // ---------------- draw ----------------
            Timer dt;
            for (auto* tr : ftracks) {
                if (tr->det_idx < 0) continue;
                const Detection& d = fd[tr->det_idx];
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
                std::vector<cv::Point> poly;
                for (auto& p : d.kps) poly.emplace_back((int)p.x, (int)p.y);
                cv::rectangle(frame, d.box, cv::Scalar(255, 160, 0), 1);
                cv::polylines(frame, poly, true, cv::Scalar(0, 255, 255), 2);
                std::string txt = it != plates.end() ? it->second.text() : "";
                int col = it != plates.end() ? it->second.color() : PC_UNKNOWN;
                char head[64]; snprintf(head, sizeof(head), "P%d%s ", tr->id, d.cls == 1 ? "(双)" : "");
                bool ok = it != plates.end() && it->second.valid();
                std::string label = std::string(head) + (txt.empty() ? "..." : txt) + (ok || txt.empty() ? "" : "?") + " " + (col >= 0 ? plate_color_cn(col) : "");
                cv::Scalar bg = color_bgr(col);
                cv::Scalar fg = (col == PC_WHITE || col == PC_YELLOW) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);
                int th = std::max(18, H / 28);
                put_text(frame, label, cv::Point((int)d.box.x, std::max(0, (int)d.box.y - th - 6)), th, fg, &bg, 3);
            }
            // 左上角耗时面板（显示上一帧完整耗时 + 本帧算法耗时）
            win.push_back(algo_ms); if (win.size() > 30) win.erase(win.begin());
            double avg = 0; for (double v : win) avg += v; avg /= win.size();
            {
                auto g = [&](const char* k) { auto i = t.find(k); return i == t.end() ? 0.0 : i->second; };
                std::vector<std::string> lines;
                char b[160];
                snprintf(b, sizeof(b), "frame %d  algo %.1f ms  FPS(algo) %.1f  FPS(e2e) %.1f", fidx, algo_ms, 1000.0 / std::max(avg, 1e-3),
                         last.count("frame_total") ? 1000.0 / std::max(last["frame_total"], 1e-3) : 0.0);
                lines.push_back(b);
                if (face_det) { snprintf(b, sizeof(b), "face det  pre %.1f infer %.1f post %.1f ms  n=%d trk=%d", g("face_pre"), g("face_infer"), g("face_post"), (int)fd.size(), (int)ftracks.size()); lines.push_back(b); }
                if (plate_det) {
                    snprintf(b, sizeof(b), "plate det pre %.1f infer %.1f post %.1f ms  n=%d trk=%d", g("plate_pre"), g("plate_infer"), g("plate_post"), (int)pd.size(), (int)ptracks.size()); lines.push_back(b);
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
            writer.write(frame);
            t["encode"] = dt.lap();
            t["frame_total"] = ft.total();
            for (auto& kv : t) g_stats[kv.first].add(kv.second);
            last = t;
            if (!a.quiet && (fidx % 30 == 0))
                printf("[frame %d/%d] algo %.1f ms, faces %zu, plates %zu (tracks %zu/%zu)\n", fidx, total, algo_ms, fd.size(), pd.size(), ftracks.size(), ptracks.size());
        }
        // 视频结束，输出所有剩余 track
        std::vector<int> ids;
        for (auto& kv : faces) ids.push_back(kv.first);
        for (int id : ids) finish_face(id);
        ids.clear();
        for (auto& kv : plates) ids.push_back(kv.first);
        for (int id : ids) finish_plate(id);
        writer.release();

        int n = fidx + 1;
        printf("\n================ summary ================\n");
        printf("frames: %d, wall %.1f s, output: %s\n", n, wall.total() / 1000.0, a.out.c_str());
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
        printf("plate tracks (hits>=%d, 有合法车牌号): %zu\n", a.min_hits, plate_summary.size());
        for (auto& s : plate_summary) printf("  %s\n", s.c_str());
        if (!a.snap_dir.empty()) printf("snapshots: %d faces, %d plates -> %s\n", face_saved, plate_saved, a.snap_dir.c_str());
    }
    backend_global_deinit();
    return rc;
}
