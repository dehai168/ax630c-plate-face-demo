// 诊断工具：逐帧导出原始检测（不过跟踪），用于分析漏检/误检
// 输出行: P frame score cls x y w h kx0 ky0 ... kx3 ky3   /  F frame score x y w h kx0 ky0 ... kx4 ky4
#include <cstdio>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include "detector.h"
using namespace pfd;
static const std::vector<float> kAnchors = {4, 5, 8, 10, 13, 16, 23, 29, 43, 55, 73, 105, 146, 217, 231, 300, 335, 433};
int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: %s video face.onnx plate.onnx out.txt [thr=0.05]\n", argv[0]); return 1; }
    float thr = argc > 5 ? std::stof(argv[5]) : 0.05f;
    backend_global_init();
    NormConfig n; n.to_rgb = true;
    YoloKpDetector fd(5, 1, kAnchors), pd(4, 2, kAnchors);
    fd.pad_value = 114; pd.pad_value = 0;
    if (!fd.load(argv[2], n) || !pd.load(argv[3], n)) return 2;
    cv::VideoCapture cap(argv[1]);
    FILE* o = fopen(argv[4], "w");
    cv::Mat f; int i = -1;
    while (cap.read(f)) {
        ++i;
        for (auto& d : pd.detect(f, thr, 0.45f)) {
            fprintf(o, "P %d %.4f %d %.1f %.1f %.1f %.1f", i, d.score, d.cls, d.box.x, d.box.y, d.box.width, d.box.height);
            for (auto& k : d.kps) fprintf(o, " %.1f %.1f", k.x, k.y);
            fprintf(o, "\n");
        }
        for (auto& d : fd.detect(f, thr, 0.45f)) {
            fprintf(o, "F %d %.4f 0 %.1f %.1f %.1f %.1f", i, d.score, d.box.x, d.box.y, d.box.width, d.box.height);
            for (auto& k : d.kps) fprintf(o, " %.1f %.1f", k.x, k.y);
            fprintf(o, "\n");
        }
        if (i % 1000 == 0) { fprintf(stderr, "%d\n", i); fflush(o); }
    }
    fclose(o);
    return 0;
}
