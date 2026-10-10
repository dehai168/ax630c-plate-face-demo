// image_eval: 对图片目录逐张跑与 plate_face_demo 完全相同的车牌(检测->四角点拉正/双层拼接->rpv3识别->颜色)和人脸检测
// 输出 TSV 到 stdout，叠加图写到 vis_dir
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "detector.h"
#include "plate_rec.h"
#include "text_render.h"
#include "timer.h"
using namespace pfd;
static const std::vector<float> kAnchors = {4, 5, 8, 10, 13, 16, 23, 29, 43, 55, 73, 105, 146, 217, 231, 300, 335, 433};
static cv::Scalar color_bgr(int c) {
    switch (c) { case PC_BLUE: return {200, 80, 0}; case PC_YELLOW: return {0, 200, 255}; case PC_GREEN: return {60, 200, 60}; case PC_YELLOWGREEN: return {40, 220, 170};
        case PC_WHITE: return {235, 235, 235}; case PC_BLACK: return {30, 30, 30}; default: return {128, 128, 128}; }
}
int main(int argc, char** argv) {
    if (argc < 8) { fprintf(stderr, "usage: %s list.txt vis_dir face.onnx plate.onnx rec.onnx cls.onnx plate_thr [face_thr]\n", argv[0]); return 1; }
    std::string list = argv[1], vis = argv[2];
    float plate_thr = std::stof(argv[7]), face_thr = argc > 8 ? std::stof(argv[8]) : 0.5f, nms_thr = 0.45f;
    backend_global_init();
    NormConfig det_norm; det_norm.to_rgb = true;
    YoloKpDetector face_det(5, 1, kAnchors); face_det.pad_value = 114;
    YoloKpDetector plate_det(4, 2, kAnchors); plate_det.pad_value = 0;
    PlateRecognizer rec;
    if (getenv("YELLOWGREEN_RULE") && atoi(getenv("YELLOWGREEN_RULE")) == 0) rec.cfg.yellowgreen_rule = false;   // v2.3 对比用
    if (getenv("LAYER_BY_COLOR") && atoi(getenv("LAYER_BY_COLOR")) == 0) rec.cfg.layer_by_color = false;   // v2.1 对比用
    if (!face_det.load(argv[3], det_norm) || !plate_det.load(argv[4], det_norm) || !rec.load(argv[5], argv[6])) { fprintf(stderr, "load fail\n"); return 3; }
    FILE* fl = fopen(list.c_str(), "r");
    char line[4096];
    // warmup
    { cv::Mat z(720, 1280, CV_8UC3, cv::Scalar(0)); plate_det.detect(z, 0.5f, 0.45f); face_det.detect(z, 0.5f, 0.45f); }
    while (fgets(line, sizeof(line), fl)) {
        std::string path(line); while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
        if (path.empty()) continue;
        std::string name = path.substr(path.find_last_of('/') + 1);
        cv::Mat img = cv::imread(path);
        if (img.empty()) { printf("IMG\t%s\tREAD_FAIL\n", name.c_str()); fflush(stdout); continue; }
        Timer t;
        auto pd = plate_det.detect(img, plate_thr, nms_thr);
        double t_det = t.lap();
        std::vector<PlateRecResult> rr;
        std::vector<int> small;
        for (auto& d : pd) {
            int layer = d.cls == 1 ? 1 : 0;
            if (d.box.width >= 24 && d.box.height >= 8) { rr.push_back(rec.recognize(img, d.kps, layer)); small.push_back(0); }
            else { rr.push_back(PlateRecResult()); small.push_back(1); }
        }
        double t_rec = t.lap();
        auto fd = face_det.detect(img, face_thr, nms_thr);
        double t_face = t.lap();
        printf("IMG\t%s\t%dx%d\t%zu\t%zu\t%.2f\t%.2f\t%.2f\n", name.c_str(), img.cols, img.rows, pd.size(), fd.size(), t_det, t_rec, t_face);
        // draw
        int H = img.rows;
        for (auto& d : fd) cv::rectangle(img, d.box, cv::Scalar(0, 255, 0), std::max(2, H / 400));
        for (size_t i = 0; i < pd.size(); ++i) {
            auto& d = pd[i]; auto& r = rr[i];
            printf("PLATE\t%zu\t%s\t%d\t%.4f\t%s\t%.4f\t%s\t%.4f\t%d\t%.0f,%.0f,%.0f,%.0f\t%d\n", i, r.text.c_str(), d.cls, d.score,
                   r.color >= 0 ? plate_color_cn(r.color) : "", r.color_conf, r.text.empty() ? "" : "x", r.conf, r.valid_format ? 1 : 0,
                   d.box.x, d.box.y, d.box.width, d.box.height, small[i]);
            printf("LAYER\t%zu\t%d\t%d\t%d\t%s\t%s\t%s\n", i, d.cls, r.layer, r.layer_overridden ? 1 : 0, r.pre_color >= 0 ? plate_color_cn(r.pre_color) : "-",
                   r.cls_color >= 0 ? plate_color_cn(r.cls_color) : "-", r.color_src);
            std::vector<cv::Point> poly; for (auto& p : d.kps) poly.emplace_back((int)p.x, (int)p.y);
            cv::rectangle(img, d.box, cv::Scalar(255, 160, 0), std::max(1, H / 600));
            cv::polylines(img, poly, true, cv::Scalar(0, 255, 255), std::max(2, H / 400));
            char head[64]; snprintf(head, sizeof(head), "%s%.2f ", r.layer == 1 ? "(双)" : "", d.score);
            std::string label = std::string(head) + (r.text.empty() ? "?" : r.text) + (r.valid_format ? "" : "?") + " " + (r.color >= 0 ? plate_color_cn(r.color) : "");
            char c2[32]; snprintf(c2, sizeof(c2), " rec%.2f", r.conf); label += c2;
            cv::Scalar bg = color_bgr(r.color);
            cv::Scalar fg = (r.color == PC_WHITE || r.color == PC_YELLOW || r.color == PC_YELLOWGREEN) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);
            int th = std::max(22, H / 30);
            int y = (int)d.box.y - th - 8; if (y < 0) y = std::min(H - th - 4, (int)(d.box.y + d.box.height + 4));
            int x = std::min(std::max(0, (int)d.box.x), std::max(0, img.cols - text_size(label, th).width - 6));
            put_text(img, label, cv::Point(x, y), th, fg, &bg, 3);
            if (getenv("CROP_DIR") && !r.rectified.empty()) cv::imwrite(std::string(getenv("CROP_DIR")) + "/" + name + "_" + std::to_string(i) + ".png", r.rectified);
            // 拉正小图贴到左上角
            if (!r.rectified.empty()) {
                cv::Mat s; int sw = std::max(160, img.cols / 6); cv::resize(r.rectified, s, cv::Size(sw, sw * r.rectified.rows / std::max(1, r.rectified.cols)));
                int yy = 4 + (int)i * (s.rows + 6);
                if (yy + s.rows < img.rows && s.cols + 4 < img.cols) s.copyTo(img(cv::Rect(4, yy, s.cols, s.rows)));
            }
        }
        std::vector<int> q = {cv::IMWRITE_JPEG_QUALITY, 85};
        cv::imwrite(vis + "/" + name, img, q);
        fflush(stdout);
    }
    return 0;
}
