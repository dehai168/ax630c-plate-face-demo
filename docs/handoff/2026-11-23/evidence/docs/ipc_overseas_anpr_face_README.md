# IPC 海外车牌识别 + 人脸抓拍算法

车载 IPC 端侧算法：海外车牌检测与识别、人脸检测与最佳帧抓拍。算法库输入 YUV 帧和参数，输出检测和识别结果，嵌入现有 IPC 框架。

- 芯片：AX631 > AX630C > AX620Q > Hi3516DV500 > AX620A
- 区域：EU、UK、巴西 → 新加坡、墨西哥、塞尔维亚、南非
- 指标：≥15 fps，检出率 ≥95%，识别准确率 ≥98%，误报 <0.5 次/小时，10 m 内识别（4 mm 镜头 5 m）

方案与工作清单：[docs/overseas_anpr_face_plan.md](docs/overseas_anpr_face_plan.md)

## 目录
```
include/        对外 C API
src/core/       pipeline
src/backends/   推理后端
src/platform/   硬件裁剪缩放
regions/        区域插件
tools/          评测与打包
models/         模型卡与下载脚本
training/       训练、合成、量化
docs/           文档
```

## 构建（x86）
```bash
sudo apt install g++ make cmake libopencv-dev
./build_x86.sh              # 下载 ONNX Runtime 和模型、编译、跑单元测试；算法包输出到 build/pkg
./build/ovs_runner --pkg build/pkg --input video.mp4 --out out/v --region sg     # 视频：overlay.mp4 + frames.jsonl + final.json
./build/ovs_runner --pkg build/pkg --input imgs/ --out out/i --image             # 图片：vis/ + results.json
python3 tools/eval.py --gt gt.tsv --results out/i/results.json
```
接口见 [include/ovs_anpr.h](include/ovs_anpr.h)，参数见 [docs/param.md](docs/param.md)，基线见 [docs/milestone1_baseline.md](docs/milestone1_baseline.md)，第三方授权见 [third_party/NOTICE.md](third_party/NOTICE.md)。

板端（AX630C / AX631 / AX620Q）：
```bash
python3 tools/prepare_ax_onnx.py && tools/pulsar2/compile_axmodels.sh ax630c     # 需带授权的 Pulsar2
TOOLCHAIN_DIR=... BSP_MSP_DIR=... OPENCV_DIR=... ./build_ax.sh ax630c
```
板上测试步骤见 docs/board_test.md。项目状态与待办见 [docs/TODO.md](docs/TODO.md)。

状态：P0 里程碑 2 —— AXEngine 后端 + IVPS 零拷贝 + Pulsar2 脚本 + 检测外扩修正 + CI（docs/milestone2.md）；板上验证待做。
