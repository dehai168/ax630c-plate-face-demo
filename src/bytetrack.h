// ByteTrack (Zhang et al. 2021) 精简 C++ 实现：卡尔曼(xyah 8 维匀速) + IoU 代价 + 匈牙利匹配 + 高/低分两阶段关联
#pragma once
#include <vector>
#include <opencv2/core.hpp>

namespace pfd {

struct KalmanXYAH {
    cv::Matx<float, 8, 1> mean;
    cv::Matx<float, 8, 8> cov;
    void initiate(const cv::Rect2f& r);
    void predict();
    void update(const cv::Rect2f& r);
    cv::Rect2f rect() const;
};

enum TrackState { TS_NEW = 0, TS_TRACKED, TS_LOST, TS_REMOVED };

struct STrack {
    int id = 0;
    TrackState state = TS_NEW;
    bool activated = false;
    KalmanXYAH kf;
    float score = 0.f;
    int start_frame = 0, frame_id = 0, tracklet_len = 0;
    int det_idx = -1;            // 本帧匹配到的检测下标（-1 表示本帧未匹配）
    cv::Rect2f rect() const { return kf.rect(); }
    int end_frame() const { return frame_id; }
};

struct ByteTrackConfig {
    float track_thresh = 0.5f;   // 高分检测阈值
    float low_thresh = 0.1f;     // 低分检测下限（第二阶段关联）
    float new_track_thresh = 0.6f;
    float match_thresh = 0.8f;   // 第一阶段 1-IoU 代价上限
    float second_match_thresh = 0.5f;
    float unconfirmed_match_thresh = 0.7f;
    int max_lost = 30;           // 丢失多少帧后删除（≈ fps * 秒）
};

class ByteTracker {
public:
    explicit ByteTracker(const ByteTrackConfig& cfg = {}) : cfg_(cfg) {}
    // boxes/scores: 本帧全部检测（含低分）；返回当前处于 Tracked 状态且已激活的轨迹
    std::vector<STrack*> update(const std::vector<cv::Rect2f>& boxes, const std::vector<float>& scores);
    // 本次 update 中被删除的轨迹（用于输出 best-shot）
    const std::vector<STrack>& removed_this_frame() const { return removed_now_; }
    const std::vector<STrack>& tracked() const { return tracked_; }
    const std::vector<STrack>& lost() const { return lost_; }
    int frame_id() const { return frame_id_; }
    ByteTrackConfig cfg_;

private:
    std::vector<STrack> tracked_, lost_, removed_now_;
    int frame_id_ = 0;
    int next_id_ = 1;
};

// 匈牙利算法（矩形代价矩阵），cost > thresh 的配对视为不匹配
void linear_assignment(const std::vector<std::vector<float>>& cost, int na, int nb, float thresh,
                       std::vector<std::pair<int, int>>& matches, std::vector<int>& un_a, std::vector<int>& un_b);

}  // namespace pfd
