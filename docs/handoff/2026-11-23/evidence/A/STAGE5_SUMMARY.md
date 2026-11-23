# A 第五阶段：原视角 vs 锁定六视角 TTA 隔离 x86 ORT CPU 成本

时间：官方测量 2026-11-17T09:56:28+08:00–2026-11-17T09:57:53+08:00，时区标签：Asia/Shanghai (UTC+8)
目录：/workspace/cn_val_su/v25/delivery_20261117/A/stage5_tta_isolated_cpu_latency_20261117

## 绑定（未发明 TTA，未改仓库）

- 旧 runner：`eval_locked_onnx_geometry.py` SHA `a6335bce…15e3d8`。不能当模块 import（无 `__main__` 守卫，会重跑并写回仓库 JSON）。仅 exec 其 `evaluate()` 及以前的原文；两处已记录替换：D 绑定到 R5 目录；SessionOptions 改为 inter=1 intra=2（本实验 2CPU 帽，原文 intra=4）。warp/normalize/fusion/decode 未改写。
- TTA 策略 SHA `d3a8c30e…3a906033`，gamma=0.1，5 个 safe inset。
- accepted ONNX SHA `45b005b5…3af4ae1`，同一 ORT `CPUExecutionProvider` session。
- decoder SHA `2abe5167…1828e6b9`。
- 600 输入：冻结 npy SHA `9233e8a7…acf461` 与 R5 `test_inputs/rec/*/data.bin` 600/600 字节一致、id 顺序与 ROWS_FROZEN 一致。预处理未改。

## 测量协议

预注册轮次顺序：warmup original→TTA；M1 TTA→original；M2 original→TTA；M3 TTA→original。N=600 未削减。前向预算 600×(1+6)×4=16800，实际 16800，预检仅 session metadata、0 额外 forward，无额外精度 forward。taskset `-c 0,1`，墙钟 85.392s / 帽 300s，max RSS 141392 kB（≈0.135 GiB）。未触帽，未因空闲内存误杀。

## 文本一致性

- TTA 四轮全部 600/600 等于 accepted 600（R32 baseline，且与 R11 locked 一致）
- original 四轮自洽；与 accepted TTA 文本差异 0/600（与 R5 `changes=[]` 方向一致）。若曾不同，也不会当 bug。
- 计时前后 ONNX/策略/decoder/输入 SHA 不变

## 官方 3 轮（warmup 不计入 p50/p95）

本环境 box x86 ORT CPU，逐条为 3 轮中位数后再取 p50/p95。不是 NPU、不是板端、不是整帧。不得把理论 6 倍当实测。

逐条 pipeline 总时：

- 原视角 p50 = 5.631 ms（p95 5.751 ms）
- 六视角 TTA p50 = 26.906 ms（p95 28.242 ms）
- 真实时间差（TTA−original）p50 = 21.275 ms / 条（p95 22.490 ms）

分项 p50：

- 模型 forward：原视角 4.085 ms，TTA 24.809 ms，差 20.723 ms
- 预处理：原视角 0.032 ms，TTA 0.516 ms
- 融合：TTA 0.066 ms（原视角 0）
- Prefix 解码：原视角 1.506 ms，TTA 1.497 ms（几乎相同，因每路只解码一次）

600 条 sweep 墙钟 p50：原视角 3.401 s，TTA 16.237 s，差 12.836 s。
三轮 TTA 墙钟：20.172 / 16.137 / 16.237 s（第 1 轮偏高，已原样记录，p50 取中位）；原视角：3.453 / 3.401 / 3.379 s。

说明：forward 分项接近 6 次对 1 次的量级，但**整条流水不是 6 倍**（Prefix 约 1.5 ms 只发生一次）。因此不能用理论 6 倍代替本实测总时差。

## 接受/拒绝

**ACCEPT_MEASUREMENT_REPRODUCIBILITY**。仅测量可复现性，不是模型或发布接受，不填生产 v2.4 浮点基线格（该格仍 UNKNOWN，不影响本项）。
