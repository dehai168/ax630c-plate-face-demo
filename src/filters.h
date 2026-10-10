// v2 新增：车牌/人脸误检过滤、OSD 自学习屏蔽、严格车牌格式、轨迹合并工具
#pragma once
#include <deque>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "detector.h"

namespace pfd {

// ---------------- 严格车牌格式 ----------------
// 普通 7 位: 省 + 字母(无I/O) + 4 位[字母数字] + [字母数字|挂学警港澳]，且序号中字母 <= max_letters
// 新能源 8 位: 省 + 字母 + ([DABCEFGHJK][字母数字][数字x4] | [数字x5][DABCEFGHJK])
// 武警 WJ + ...(7~8 位)
bool strict_plate_format(const std::string& utf8, int max_serial_letters = 3);
// v2.3: 大型新能源号码（8 位，末位动力类型字母 letters 之一）
bool is_large_ne_plate(const std::string& utf8, const std::string& letters = "ABCDEFGHJK", int max_serial_letters = 3);
std::vector<std::string> utf8_chars(const std::string& s);
int utf8_edit_distance(const std::string& a, const std::string& b);

// ---------------- 车牌四角点几何 ----------------
struct PlateGeomConfig {
    float single_ar_min = 1.6f, single_ar_max = 6.5f;   // 单层牌 拉正后宽/高（标准 440/140=3.14）
    float double_ar_min = 1.0f, double_ar_max = 3.0f;   // 双层牌（标准 440/220=2.0）
    float side_ratio_max = 2.2f;                        // 对边长度比上限（透视允许一定变形）
    float min_angle_deg = 45.f;                         // 四角最小内角
    float kps_out_box = 0.25f;                          // 角点允许超出检测框的比例
    int min_width = 24;                                 // 车牌像素宽下限（太小不识别）
};
// 返回 true 表示像车牌；reason 填失败原因
bool plate_geometry_ok(const Detection& d, const PlateGeomConfig& cfg, std::string* reason = nullptr);

// ---------------- 人脸五点几何 ----------------
struct FaceGeomConfig {
    int min_size = 20;                 // 人脸框短边下限(px, 原图)
    float eye_dist_min = 0.06f;        // 两眼距离 / 框宽（侧脸可很小）
    float eye_dist_max = 0.80f;
    float kps_out_box = 0.10f;         // 关键点允许超出框的比例
    float min_score = 0.0f;            // 由调用方阈值控制
};
// 返回几何得分 [0,1]，<0 表示不合格（非人脸）
float face_geometry_score(const Detection& d, const FaceGeomConfig& cfg, std::string* reason = nullptr);

// 人脸框中部肤色像素比例（YCrCb），轮胎/衣服/暗处纹理通常 < 0.1
float face_skin_ratio(const cv::Mat& bgr, const cv::Rect2f& box);

// ---------------- OSD 自学习屏蔽 ----------------
// 原理：OSD 文字（车速/时间/"No positioning module" 等）在画面中位置固定，车载相机在移动时背景在变；
//   而真实车牌随车辆在画面中移动。
//   1) 只用"非车牌"的检测学习（所在轨迹没有稳定的合法车牌号）——避免把长时间跟车的前车车牌学成 OSD
//   2) 按位置聚类（IoU>=0.6 视为同一位置），记录每次出现（episode）时框外围背景的缩略图
//   3) 同一位置多次出现、且外围背景变化明显（说明相机在动、框却不动）的次数 >= min_changes -> OSD
//      （在同一次连续出现中，若命中 >= min_hits 且背景已明显变化，也计一次）
//   4) 学到后屏蔽该位置相近尺寸的检测，持续有效；停车等红灯（背景不变）不会被学成 OSD
// 另外支持用户配置固定屏蔽区（相对坐标 0~1 或像素）。
struct OsdConfig {
    bool enable = true;
    int episode_gap = 20;      // 间隔多少帧算一次新的出现
    int min_hits = 25;         // 单次连续出现内命中多少帧后检查背景变化
    int min_changes = 2;       // 需要的"背景变化"证据次数
    int min_total_hits = 6;    // 总命中下限
    float bg_diff_thr = 10.f;  // 外围背景平均灰度差阈值（0~255）
    float iou_same = 0.6f;
    float mask_expand = 0.3f;  // 屏蔽区外扩比例
    int forget = 3000;         // 候选多少帧未出现则遗忘
};

class OsdFilter {
public:
    explicit OsdFilter(const OsdConfig& c = {}) : cfg_(c) {}
    void add_user_region(const cv::Rect2f& r) { user_.push_back(r); }
    // 每帧调用一次：boxes 为本帧"非车牌"检测框（原图坐标）
    void update(int frame_idx, const cv::Mat& frame, const std::vector<cv::Rect2f>& boxes);
    // 检测框是否落在屏蔽区（用户 + 自学习）
    bool suppressed(const cv::Rect2f& b) const;
    const std::vector<cv::Rect2f>& learned() const { return learned_; }
    const std::vector<cv::Rect2f>& user() const { return user_; }
    OsdConfig cfg_;

private:
    struct Cand {
        cv::Rect2f box;
        int hits = 0, ep_hits = 0, changes = 0, last = -1;
        bool ep_counted = false;
        cv::Mat ring_ep;           // 本次出现开始时的外围背景
        cv::Mat ring_prev;         // 上次出现时的外围背景
    };
    cv::Mat ring_patch(const cv::Mat& gray_small, const cv::Rect2f& b) const;
    float ring_diff(const cv::Mat& a, const cv::Mat& b) const;
    std::vector<Cand> cands_;
    std::vector<cv::Rect2f> learned_, user_;
    cv::Mat small_;
    float sx_ = 1.f, sy_ = 1.f;
};

// 解析 "x,y,w,h;x,y,w,h"（值都 <=1 视为相对坐标）
std::vector<cv::Rect2f> parse_regions(const std::string& s, int W, int H);

}  // namespace pfd
