# 项目总状态

整理时间：2026-11-23T20:16:23+08:00（Asia/Shanghai（UTC+8））。下列结论来自 `/workspace/cn_val_su/v25/delivery_20261117/` 及既有 optimization_round 档案。与记忆冲突时以档案为准并标明。

## 一句话

AX630C 车牌人脸应用的生产版本仍是 **v2.4（可回退）**；**v2.5 发布 NO-GO**。A/B/C 三条线都有已接受的软件或离线证据，但四级验收（离线 / NPU 实际推理 / 板端完整视频 / 生产验收）没有同时通过。队列在 2026-11-17T11:38:26+08:00 记为 `BLOCKED_EXTERNAL_DEPENDENCIES`，`active_work` 为空，`queue_is_idle` 为 true。本交接未改该队列。

## 仓库与分支（最后已知，须提交阶段核验）

| 项 | 最后已知 | 说明 |
|---|---|---|
| 远程 | `https://github.com/dehai168/ax630c-plate-face-demo` | 早期 C 线首批报告曾写「元数据中无 remote」，后续档案已记录该 URL。以提交阶段 `git ls-remote` / `git fetch` 为准 |
| `main` | `08eb9d6` | 未当作 v2.5 已发布 |
| C 分支 | `cursor/v25-c-integration-audit-50d6` | 最后已知提交 `2241e28c110f8a10b282619459743964e8b79d4e` |
| 2241e28 内容 | 仅将旧三类 `classify_color` 的 `size()>=3` 改为 `==3` | 此前有 R40/R46 等六文件静态 replay；**未合并部署** |
| 生产源归档 | `/workspace/ax630c_plate_face_demo_v2.4_src.tar.gz` SHA `e04ff400ad5f7709b3dbfe518262baa7f8b9bd3989f4dbfe29135e98935074ba` | 不要改这份归档 |
| 生产板端包 | `/workspace/ax630c_plate_face_demo_v2.4_board.tar.gz` SHA `29c7591f72f4f8bd0d45f0fdd10ab75e5df9210800e184c0f8937ff0028e0750` | 同上 |

`2241e28` 不是生产默认，也不是发布通过。

## 三线当前结论

- **A（OCR）**：已接受软件模型 `45b005…`（ONNX）与 `f3460c34…`（PT），Prefix Beam 与几何 TTA 身份已锁定；canonical val600 文本 554/600 且与归档一致。x86 隔离时延：Prefix p50 **1.418 ms**；原视角 **5.631 ms** vs 六视角 TTA **26.906 ms**。这些**绝非板端/NPU/整帧**。生产 rec `728d8f20…` 官方 float/export 链缺失，生产 val600 软件格 **UNKNOWN**。R31 永久拒绝，不重跑。
- **B（五类颜色量化）**：canonical float `ecba573c…`，学生 `9c840305…`。R84 终端 Max **0.0468529** / MAE **0.00520556** / argmax **63/64**，原门拒绝。CAL64 就是 SOURCE64，与 464 校准重叠，不是独立 holdout。B11 误差向量抵消，不能减 norm 分摊。B14：64 PNG/raw 字节一致，但不证明历史 loader。B16：现场 preobserver 缺口。B17：唯一公开 CLI 在 session 构造时 `NoneType.hardware_type`，0/64，不得重复或 patch 私有 API。原门不变：Max≤0.03、MAE≤0.005、argmax 64/64。
- **C（集成与板）**：生产代码不动。静态审计已完成，第 96 行把仓库 `9d41` 误记为 R84 的勘误须与原文一起读。42 张几何 U8/F32 全等、52 项真实函数 mock 都是离线、非模型/NPU/video。仓库颜色 ONNX `9d41c7dc…` 与 `ecba`/`9c840` 字节不同，训练继承 UNKNOWN。canonical special-double 先横拼再 resize96；SU 是数据来源标签，不是线上标志。runtime layer 风险未闭合。夹具 21 double + 21 single、SU 0，包哈希 `a8df601e…`，角色是 TRAIN400 冻结夹具。

## 板端时间口径（不要写成今天）

**最后一次本交付档案中的实测是 2026-11-17**（Asia/Shanghai（UTC+8）），不是 2026-11-23。

2026-11-17 11:27 用户明确授权通过串口改一个 `151.x` 临时地址。COM3 上已有认证 shell（`/opt #`），该次未使用密码、未复位。向 eth0 RAM 添加 `192.168.151.200/24`，保留 `10.100.100.101/24` 与默认路由 `via 10.100.100.1`，无持久配置写入。用户机 `level` 当时为 `192.168.151.152/24`。ICMP 成功且邻居 MAC `00:18:f5:a0:a9:6e` 一致；**TCP/22 未监听，未做 SSH 登录，上轮未启动 sshd**。串口新读 ext4 `errors_count`：p13=0，p14=**2168**（当前测量，虽与历史 2168 相同但不是复用旧数）。R44 的 8 项项目文件备份不是系统镜像。

整理时补充、**未在 `delivery_20261117` 目录找到独立落盘证明**的后续说明（不得写成今日实测）：

- 2026-11-19：曾通知 COM3 消失（Code 45）。
- 2026-11-19 18:11–18:12：最后一次成功网络复核。
- 此后用户机多数离线。
- 2026-11-23 例行巡检连续因账户无 GrokBot usage 失败，意味着近期巡检未成功，不是持续在线工作的证明。

临时 `192.168.151.200` 是 RAM 地址，重启或接口重置后可能已不在。回退命令已记录且当时未执行：`ip addr del 192.168.151.200/24 dev eth0`。

## 四级验收与发布

1. 离线：有界通过（含 R40/R41/R45/R46 及后来的 42/52），不是 installed binary parity，也不是发布。
2. NPU 实际推理：旧三类有界通过（R36）；五类对参考数值失败（R50）。
3. 板端完整视频：未做。
4. 生产验收：NO-GO。v2.4 保持可回退。

## 本整理未验证

- 远端 Git 当前 HEAD 是否仍为上述提交
- 2026-11-17 之后板卡、COM3、`192.168.151.200` 是否仍存在
- 大包是否被改动（使用档案已记录哈希，未在本整理中重算 100MB 级 tar）
