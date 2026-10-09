#include "bytetrack.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "detector.h"

namespace pfd {

// ---------------- Kalman (与 ByteTrack/DeepSORT 参数一致) ----------------
static const float kStdPos = 1.f / 20, kStdVel = 1.f / 160;

void KalmanXYAH::initiate(const cv::Rect2f& r) {
    float h = std::max(r.height, 1.f);
    mean = cv::Matx<float, 8, 1>(r.x + r.width / 2, r.y + r.height / 2, r.width / h, h, 0, 0, 0, 0);
    float s[8] = {2 * kStdPos * h, 2 * kStdPos * h, 1e-2f, 2 * kStdPos * h,
                  10 * kStdVel * h, 10 * kStdVel * h, 1e-5f, 10 * kStdVel * h};
    cov = cv::Matx<float, 8, 8>::zeros();
    for (int i = 0; i < 8; ++i) cov(i, i) = s[i] * s[i];
}

void KalmanXYAH::predict() {
    float h = mean(3);
    float s[8] = {kStdPos * h, kStdPos * h, 1e-2f, kStdPos * h, kStdVel * h, kStdVel * h, 1e-5f, kStdVel * h};
    cv::Matx<float, 8, 8> F = cv::Matx<float, 8, 8>::eye();
    for (int i = 0; i < 4; ++i) F(i, i + 4) = 1.f;
    cv::Matx<float, 8, 8> Q = cv::Matx<float, 8, 8>::zeros();
    for (int i = 0; i < 8; ++i) Q(i, i) = s[i] * s[i];
    mean = F * mean;
    cov = F * cov * F.t() + Q;
}

void KalmanXYAH::update(const cv::Rect2f& r) {
    float h = mean(3);
    float s[4] = {kStdPos * h, kStdPos * h, 1e-1f, kStdPos * h};
    cv::Matx<float, 4, 8> H = cv::Matx<float, 4, 8>::zeros();
    for (int i = 0; i < 4; ++i) H(i, i) = 1.f;
    cv::Matx<float, 4, 4> R = cv::Matx<float, 4, 4>::zeros();
    for (int i = 0; i < 4; ++i) R(i, i) = s[i] * s[i];
    float zh = std::max(r.height, 1.f);
    cv::Matx<float, 4, 1> z(r.x + r.width / 2, r.y + r.height / 2, r.width / zh, zh);
    cv::Matx<float, 4, 4> S = H * cov * H.t() + R;
    cv::Matx<float, 8, 4> K = cov * H.t() * S.inv(cv::DECOMP_CHOLESKY);
    mean = mean + K * (z - H * mean);
    cov = (cv::Matx<float, 8, 8>::eye() - K * H) * cov;
}

cv::Rect2f KalmanXYAH::rect() const {
    float w = mean(2) * mean(3), h = mean(3);
    return cv::Rect2f(mean(0) - w / 2, mean(1) - h / 2, w, h);
}

// ---------------- Hungarian (Kuhn-Munkres, O(n^3)) ----------------
void linear_assignment(const std::vector<std::vector<float>>& cost, int na, int nb, float thresh,
                       std::vector<std::pair<int, int>>& matches, std::vector<int>& un_a, std::vector<int>& un_b) {
    matches.clear(); un_a.clear(); un_b.clear();
    if (na == 0 || nb == 0) {
        for (int i = 0; i < na; ++i) un_a.push_back(i);
        for (int j = 0; j < nb; ++j) un_b.push_back(j);
        return;
    }
    int n = std::max(na, nb);
    const double BIG = 1e6;
    // 方阵，超阈值的配对给高代价，等价于 lapjv 的 cost_limit
    std::vector<std::vector<double>> a(n + 1, std::vector<double>(n + 1, BIG));
    for (int i = 0; i < na; ++i)
        for (int j = 0; j < nb; ++j) a[i + 1][j + 1] = cost[i][j] > thresh ? BIG : cost[i][j];
    std::vector<double> u(n + 1), v(n + 1);
    std::vector<int> p(n + 1), way(n + 1);
    for (int i = 1; i <= n; ++i) {
        p[0] = i; int j0 = 0;
        std::vector<double> minv(n + 1, std::numeric_limits<double>::infinity());
        std::vector<char> used(n + 1, 0);
        do {
            used[j0] = 1; int i0 = p[j0], j1 = 0; double delta = std::numeric_limits<double>::infinity();
            for (int j = 1; j <= n; ++j)
                if (!used[j]) {
                    double cur = a[i0][j] - u[i0] - v[j];
                    if (cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                    if (minv[j] < delta) { delta = minv[j]; j1 = j; }
                }
            for (int j = 0; j <= n; ++j)
                if (used[j]) { u[p[j]] += delta; v[j] -= delta; } else minv[j] -= delta;
            j0 = j1;
        } while (p[j0] != 0);
        do { int j1 = way[j0]; p[j0] = p[j1]; j0 = j1; } while (j0);
    }
    std::vector<int> a_of_b(nb, -1), b_of_a(na, -1);
    for (int j = 1; j <= n; ++j) {
        int i = p[j];
        if (i >= 1 && i <= na && j <= nb && a[i][j] < BIG) { b_of_a[i - 1] = j - 1; a_of_b[j - 1] = i - 1; }
    }
    for (int i = 0; i < na; ++i) if (b_of_a[i] >= 0) matches.emplace_back(i, b_of_a[i]); else un_a.push_back(i);
    for (int j = 0; j < nb; ++j) if (a_of_b[j] < 0) un_b.push_back(j);
}

static cv::Rect2f buffered(const cv::Rect2f& r, float b) {
    if (b <= 0.f) return r;
    return cv::Rect2f(r.x - r.width * b, r.y - r.height * b, r.width * (1 + 2 * b), r.height * (1 + 2 * b));
}

static std::vector<std::vector<float>> iou_cost(const std::vector<STrack*>& ts, const std::vector<int>& di,
                                                const std::vector<cv::Rect2f>& boxes, const std::vector<float>* scores,
                                                float buffer = 0.f) {
    std::vector<std::vector<float>> c(ts.size(), std::vector<float>(di.size(), 1.f));
    for (size_t i = 0; i < ts.size(); ++i) {
        cv::Rect2f tr = buffered(ts[i]->rect(), buffer);
        for (size_t j = 0; j < di.size(); ++j) {
            float o = iou(tr, buffered(boxes[di[j]], buffer));
            if (scores) o *= (*scores)[di[j]];   // ByteTrack fuse_score
            c[i][j] = 1.f - o;
        }
    }
    return c;
}

std::vector<STrack*> ByteTracker::update(const std::vector<cv::Rect2f>& boxes, const std::vector<float>& scores) {
    ++frame_id_;
    removed_now_.clear();
    std::vector<int> high, low;
    for (int i = 0; i < (int)boxes.size(); ++i) {
        if (scores[i] >= cfg_.track_thresh) high.push_back(i);
        else if (scores[i] > cfg_.low_thresh) low.push_back(i);
    }
    for (auto& t : tracked_) t.det_idx = -1;
    for (auto& t : lost_) t.det_idx = -1;

    // 候选: 已确认的 tracked + lost，未确认的单独处理
    std::vector<STrack*> pool, unconfirmed;
    for (auto& t : tracked_) (t.activated ? pool : unconfirmed).push_back(&t);
    for (auto& t : lost_) pool.push_back(&t);
    for (auto* t : pool) t->kf.predict();
    for (auto* t : unconfirmed) t->kf.predict();

    auto apply = [&](STrack* t, int di) {
        t->kf.update(boxes[di]);
        t->score = scores[di];
        t->det_idx = di;
        t->hits++;
        t->frame_id = frame_id_;
        t->tracklet_len = (t->state == TS_TRACKED) ? t->tracklet_len + 1 : 0;
        t->state = TS_TRACKED;
        t->activated = true;
    };

    // 第一阶段: 高分检测 vs (tracked+lost)
    std::vector<std::pair<int, int>> m; std::vector<int> ua, ub;
    linear_assignment(iou_cost(pool, high, boxes, cfg_.fuse_score ? &scores : nullptr, cfg_.buffer), (int)pool.size(), (int)high.size(), cfg_.match_thresh, m, ua, ub);
    for (auto& p : m) apply(pool[p.first], high[p.second]);
    std::vector<int> high_left; for (int j : ub) high_left.push_back(high[j]);

    // 第二阶段: 低分检测 vs 第一阶段剩下的 tracked（不含 lost）
    std::vector<STrack*> remain;
    for (int i : ua) if (pool[i]->state == TS_TRACKED) remain.push_back(pool[i]);
    std::vector<STrack*> lost_candidates;
    for (int i : ua) if (pool[i]->state != TS_TRACKED) lost_candidates.push_back(pool[i]);
    linear_assignment(iou_cost(remain, low, boxes, nullptr, cfg_.buffer), (int)remain.size(), (int)low.size(), cfg_.second_match_thresh, m, ua, ub);
    for (auto& p : m) apply(remain[p.first], low[p.second]);
    for (int i : ua) remain[i]->state = TS_LOST;   // 标记丢失

    // 未确认轨迹 vs 剩余高分检测
    linear_assignment(iou_cost(unconfirmed, high_left, boxes, cfg_.fuse_score ? &scores : nullptr, cfg_.buffer), (int)unconfirmed.size(), (int)high_left.size(), cfg_.unconfirmed_match_thresh, m, ua, ub);
    for (auto& p : m) apply(unconfirmed[p.first], high_left[p.second]);
    for (int i : ua) unconfirmed[i]->state = TS_REMOVED;
    std::vector<int> new_dets; for (int j : ub) new_dets.push_back(high_left[j]);

    // 重新整理列表
    std::vector<STrack> nt, nl;
    auto sort_into = [&](std::vector<STrack>& src) {
        for (auto& t : src) {
            if (t.state == TS_TRACKED) nt.push_back(t);
            else if (t.state == TS_LOST) {
                if (frame_id_ - t.frame_id > cfg_.max_lost) { t.state = TS_REMOVED; removed_now_.push_back(t); }
                else nl.push_back(t);
            } else if (t.state == TS_REMOVED) {
                if (t.activated && t.tracklet_len > 0) removed_now_.push_back(t);
            }
        }
    };
    sort_into(tracked_);
    sort_into(lost_);
    // 新建轨迹
    for (int di : new_dets) {
        if (scores[di] < cfg_.new_track_thresh) continue;
        STrack t;
        t.kf.initiate(boxes[di]);
        t.id = next_id_++;
        t.score = scores[di];
        t.start_frame = t.frame_id = frame_id_;
        t.state = TS_TRACKED;
        t.activated = (frame_id_ == 1);
        t.det_idx = di;
        t.hits = 1;
        nt.push_back(t);
    }
    // 去重：tracked 与 lost IoU 过高时，保留时间更长的（ByteTrack remove_duplicate_stracks）
    for (auto it = nl.begin(); it != nl.end();) {
        bool dup = false;
        for (auto& t : nt)
            if (iou(t.rect(), it->rect()) > 0.85f) { dup = true; break; }
        if (dup) { it->state = TS_REMOVED; removed_now_.push_back(*it); it = nl.erase(it); } else ++it;
    }
    tracked_.swap(nt);
    lost_.swap(nl);
    std::vector<STrack*> out;
    for (auto& t : tracked_) if (t.activated) out.push_back(&t);
    return out;
}

}  // namespace pfd
