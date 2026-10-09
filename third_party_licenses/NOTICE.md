# 第三方组件声明
- FFmpeg 6.1.2 —— LGPL-2.1-or-later，以动态库形式随包分发（lib/*.so）。
  源码：https://ffmpeg.org/releases/ffmpeg-6.1.2.tar.xz
  配置：见 scripts/build_3rdparty_aarch64.sh（--enable-shared，未开启 --enable-gpl / --enable-nonfree）。
  可以用同 ABI 的 .so 自行替换。
- OpenCV 4.10.0 —— Apache-2.0，静态链接。
- HyperLPR3 模型（车牌检测/识别/颜色）—— Apache-2.0，https://github.com/szad670401/HyperLPR
- yolov5-face 权重 yolov5n-face —— 上游代码为 GPL-3.0（https://github.com/deepcam-cn/yolov5-face），
  训练数据为 WIDER FACE（非商业研究用途）。仅供评估测试，商用前需更换或取得授权。
- Noto Sans CJK —— SIL Open Font License 1.1（叠加文字字形，已预渲染进程序）。
- AXERA AX620E SDK 运行库（lib_sdk_fallback/）—— 版权归 AXERA，按 AXERA 授权条款使用。
