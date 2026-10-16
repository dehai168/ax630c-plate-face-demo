# AX630C 人脸 + 车牌 全流程 Demo（板端运行包 v2.4）

v2.4：默认 PFD_VNPU=1 和 /usr/local/lib；npu1 默认使用 U16 车牌检测模型 models/plate_det_npu1_u16.axmodel；单/双层双假设识别（demo.cfg dual_hyp）。改动与评测数字见 CHANGELOG.md，文件校验见 SHA256SUMS。`./plate_face_demo --version` 查看版本。

完整说明（编译、模型、license、已知限制）见源码包里的 README.md。

## 目录
```
plate_face_demo        主程序 (aarch64, glibc>=2.29, libstdc++ 已静态链接)
video_io_selftest      视频读写自检工具（不用 NPU）
run.sh                 一键运行脚本
lib/                   FFmpeg 6.1.2 动态库 (LGPL-2.1)
lib_sdk_fallback/      AX620E SDK V2.0.0_P7 的 libax_engine/libax_interpreter/libax_sys，仅在板子系统里没有这些库时使用
models/                face_det / plate_det / plate_rec / plate_cls 的 npu1、npu2 两套 axmodel (Pulsar2 8.0, target AX620E)
test_video.mp4         测试视频 1280x720 15fps 48s
third_party_licenses/  第三方组件声明（FFmpeg LGPL 等）
```

## 运行
```sh
tar xzf ax630c_plate_face_demo_v2.4_board.tar.gz && cd ax630c_plate_face_demo_board
sha256sum -c SHA256SUMS                            # 可选：校验
./video_io_selftest test_video.mp4 /tmp/t.mp4     # 可选：先确认视频读写 OK
./run.sh                                           # test_video.mp4 -> out.mp4，最佳帧在 snaps/
./run.sh my.mp4 my_out.mp4                         # 自己的视频
NPU=npu2 ./run.sh                                  # 用 npu2 版本模型
EXTRA_ARGS="--max_frames 100 --quiet" ./run.sh     # 透传参数
```
输出为 .avi 时用 MJPG 编码，其它后缀用 mp4v (MPEG-4 Part 2)。结束时打印各模块平均耗时。

单模型纯 NPU 耗时请用板子自带的 `ax_run_model -m models/xxx_npu1.axmodel -r 100` 测。
