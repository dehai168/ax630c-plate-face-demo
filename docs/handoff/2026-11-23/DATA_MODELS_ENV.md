# 数据、模型与环境资产（不入本 Git 包）

全量模型、数据、备份和夹具压缩包**不要提交到本交接 Git 文档包**。下表给出原路径、用途、已知字节哈希、接手访问条件和缺失状态。哈希来自既有清单或交付档案；本整理**没有**为列哈希而重算超大包，也**没有**递归访问 `private_root`。源文件若此时不存在，记为 MISSING；哈希若档案未记载，记为 UNKNOWN，不伪造。

访问条件：这些路径位于共享整理机（box）文件系统。用户 Windows 机与板卡上的路径需要相应机器在线且已获授权才能复核。生产板上的 `/opt/demo_v24/...` 只在历史串口/备份记录中验证过。

## 生产与源归档

| 路径 | 用途 | 字节（若知） | SHA256 | 状态 |
|---|---|---|---|---|
| `/workspace/ax630c_plate_face_demo_v2.4_src.tar.gz` | 生产 v2.4 源归档，勿改 | 101411744 | `e04ff400ad5f7709b3dbfe518262baa7f8b9bd3989f4dbfe29135e98935074ba` | 整理时存在；哈希来自 SHA256SUMS，本包未重算 |
| `/workspace/ax630c_plate_face_demo_v2.4_board.tar.gz` | 生产 v2.4 板端包，勿改 | 39177989 | `29c7591f72f4f8bd0d45f0fdd10ab75e5df9210800e184c0f8937ff0028e0750` | 同上 |
| `/workspace/ax630c_plate_face_demo_v2.4_SHA256SUMS.txt` | 上述两包清单 | — | 见文件本身 | 存在 |
| `/workspace/ax630c_plate_face_demo` | 现场 checkout，含 `.git` | — | 不要擅自当作可改生产树 | 存在；历史要求不深入改动 |

## OCR / 识别模型与锁定输入

| 路径 | 用途 | SHA256 | 状态 |
|---|---|---|---|
| `/workspace/cn_val_su/finetune_original_v2/plate_rec_finetune.onnx` | 已接受软件 ONNX | `45b005b56e92df83bbba2199e97935c7897ff23996905d6e6eed0b3ea3af4ae1` | 档案 2026-11-17 已复核 |
| `/workspace/cn_val_su/finetune_original_v2/best_rec_finetune.pt` | 已接受软件 PT | `f3460c3485fa0b4c53217ab5604512dc4d119422823f3f5844fdd77caa27a272` | 同上 |
| `/workspace/cn_val_su/v25/optimization_round31/single_final_candidate_PENDING_CAL.pt` | R31 拒绝候选 | `0565e53aed3d0465b126c5d3b971737fad721f0c093caf20781c2625d8d90814` | 不要重跑 |
| `/workspace/cn_val_su/v25/optimization_round32/canonical_val600_ROWS_FROZEN.json` | val600 行锁 | `f3b71ed8459264fa5b32a3afef2163e05cda89b093f44f39a710b0607acab1e3` | 档案已复核 |
| `/workspace/cn_val_su/v25/optimization_round32/canonical_val600_original_prepare_uint8.npy` | val600 prepared u8 | `9233e8a740c61b828b81f5b24f776518997663f7452309fa8daa9875acacf461` | 档案已复核 |
| `/workspace/cn_val_su/v25/optimization_round4/decoder_core.py` | 锁定 Prefix Beam | `2abe5167acfac28f30b9589d344cf681cadf7cdba2ddceb0a86b36ae1828e6b9` | 档案已复核 |
| `/workspace/cn_val_su/v25/optimization_round5/train_locked_geometry_policy.json` | 锁定 TTA | `d3a8c30e8392707ca98037ec5b023189c81466593ccf833a4fd79bbb3a906033` | 档案已复核 |
| 生产 `plate_rec_npu1.axmodel` | 默认生产 rec | `728d8f2065632dbd383c347d80f4fba12a38d270da9289223cc709285ba66576` | 身份已绑定；官方 float 源 UNKNOWN |
| 板外 `improve/stage/orig_rec.axmodel` 等 | 与 728d8f20 对照的已有副本 | 同 728d8f20（档案） | 以 stage3 报告为准 |

## 颜色模型与校准

| 路径 | 用途 | SHA256 | 状态 |
|---|---|---|---|
| `/workspace/cn_val_su/finetune_original_v2/plate_color_finetune.onnx` | canonical 原 float（3367662 B） | `ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360` | 档案已测 |
| `/workspace/cn_val_su/v25/optimization_round82/work/plate_color_qat_student.onnx` | R82/R84 学生 | `9c840305ea6f5cb439b78ac350c05cb2f1674357bb47652e34f2f9de83600827` | 档案已测 |
| `/workspace/cn_val_su/v25/optimization_round84/work/candidate_build/compiled.axmodel` | R84 编译产物（失败门禁） | `f16afae621ba3fa54cee3ab9ac062b3e26925e6497d469fc342867d8c7c4f5a4` | 不要当产品默认 |
| 仓库 `models/onnx/plate_color_finetune.onnx` | 树上 9d41 颜色 ONNX | `9d41c7dc…`（完整见绑定矩阵与审计表） | 与 ecba/9c840 不同；训练继承 UNKNOWN |
| TRAIN400 manifest | 夹具与校准角色 | `db2c85bd16fe777cd39bdff8d1fdf96f5e9ade4ce263bdc35c510becff8435e7` | 见夹具交接 |
| `/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/preprocess_fixtures.tar.gz` | 21+21 TRAIN 夹具（1677077 B） | `a8df601e060874357cad83f947abcd8a71361a2e7816811de0b9b6a4122746d9` | 整理时存在；不入本包 |
| `/workspace/cn_val_su/original_mixed_finetune_20261107.tar.gz` | 训练交付同名包（9057105 B），无训练器 | `c1d86ea45b6037a9c3fa0a396b4d66716da65d26b59e91620993ca39194e4f10` | 合同原路径曾缺失，档案用此路径 |
| `/workspace/cn_val_su/v25/calib_color.tar` 等 | 历史校准包 | 见各 manifest | 在 v25 目录；用途见 CAL64 角色说明 |

颜色 6400 份 raw（约 1.23e9 B）的完整性在 B 首批已复核；本包不收录。

## 备份

| 项 | 说明 |
|---|---|
| R44 8/8 项目文件备份 | 7,862,564 B，私有 ZIP SHA `4a684c9a9d91a70fb722df49df9ead06413d4806c84ea5981b434d1847857c9f`。**不是系统备份**，不入本包，不用于擅自恢复 |
| R47 系统级备份准备 | 准备完成，外部救援路径受阻，**无系统镜像** |
| 板端 `/opt`、p14 | 有历史只读与 errors_count；无授权不得写 |

## 环境与机器（历史）

- 整理机（box）：本包与交付档案所在处。
- 用户 Windows：标签 `level`，machineId `697781b2-8ff0-4a80-a5ef-b65276461177`。2026-11-17 以太网 `192.168.151.152/24`。之后是否在线须新确认。
- 板卡 AX630C：历史 eth0 `10.100.100.101/24`，MAC `00:18:f5:a0:a9:6e`；2026-11-17 RAM 地址 `192.168.151.200/24`。COM3 Prolific `VID_067B&PID_2303` InstanceId `8&34BE1EAC&1&3`。这些都是历史观察。

## 缺失或本整理未验证

- 生产 rec 的官方 float/export 文件与 manifest
- 合同路径 `cn_plate_special_20261107/.../original_mixed_finetune_20261107.tar.gz`（已用 SHA 匹配的迁址包代替）
- 系统镜像（0，未取得）
- 远端 Git 当前是否仍等于最后已知提交（未在整理阶段 fetch）
- 2026-11-17 之后板卡地址/COM3 是否仍有效
- `private_root` 与发行激活材料：有意不访问、不列哈希
