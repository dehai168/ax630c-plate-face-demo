// 板端 OpenCV 视频读写自检: ./video_io_selftest in.mp4 out.mp4 [N]
#include <cstdio>
#include <cstdlib>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: %s in.mp4 out.mp4|out.avi [N]\n", argv[0]); return 1; }
    int N = argc > 3 ? atoi(argv[3]) : 30;
    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) { printf("open input failed\n"); return 2; }
    int W = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH), H = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(cv::CAP_PROP_FPS);
    std::string o = argv[2];
    int cc = o.size() > 4 && o.substr(o.size() - 4) == ".avi" ? cv::VideoWriter::fourcc('M','J','P','G') : cv::VideoWriter::fourcc('m','p','4','v');
    cv::VideoWriter w(o, cc, fps, cv::Size(W, H));
    if (!w.isOpened()) { printf("open output failed\n"); return 3; }
    cv::Mat f; int n = 0;
    while (n < N && cap.read(f)) { cv::putText(f, std::to_string(n), {20, 60}, 0, 2, {0, 0, 255}, 3); w.write(f); ++n; }
    printf("backend=%s %dx%d@%.2f, wrote %d frames to %s\n", cap.getBackendName().c_str(), W, H, fps, n, o.c_str());
    return n > 0 ? 0 : 4;
}
