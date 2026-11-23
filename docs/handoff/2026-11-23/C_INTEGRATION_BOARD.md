# C 线：工程集成与板端

证据副本：[evidence/C/](evidence/C/)、[evidence/handoff/](evidence/handoff/)。

## 原则

- 生产 v2.4 代码与生产部署保持可回退，本线候选不得写成已上生产。
- 四个验收层级不可互相替代：离线 ≠ NPU 实际推理 ≠ 板端完整视频 ≠ 生产验收。
- 已通过的 176 / 32 / 43 fixture / 54 main / 42 几何 / 52 mock **不要无意义重跑**。

## 代码与仓库

- 独立分支最后已知：`cursor/v25-c-integration-audit-50d6` @ `2241e28c110f8a10b282619459743964e8b79d4e`。
- `2241e28` 仅将旧三类 `classify_color` 逻辑中 `outs[0].size() >= 3` 改为 `== 3`（fail-closed 误接线保护）。此前有 R40/R46 等六源文件静态无漂移 replay。**未合并部署。**
- `plate_rec.cpp` 在该提交后的记录 SHA：`2f1eedd78dd5705e4400ede44027098224ca1cf5239bc0d3e98dfc3cb45857a0`（以档案为准；提交阶段再核远端）。
- 应用源归档：`/workspace/ax630c_plate_face_demo_v2.4_src.tar.gz` SHA `e04ff400…`。派生补丁在 [evidence/handoff/](evidence/handoff/)：R40 `INTEGRATION_EXACT.diff` SHA `4eaac6ac5248029b14853ac84fc84400a6550dcd6a820bb43bba5ebda328970a`；R46 `MINIMAL_DERIVED_FORMAT_FIX.diff` SHA `2b6d511bec7d5b46e1af7415fa973de9347d1e74248cdd8a7bcd25602766b470`。`git apply` 需 `-p1`（不是 `-p2`）。R40 补丁不创建 `color5_crop_policy.inc`，交接包里有配套 inc。
- 早期首批报告曾写「元数据中无 remote URL」。后续档案已记录 `https://github.com/dehai168/ax630c-plate-face-demo`。以提交阶段标准 git 核验为准。

## 静态审计与勘误

- C-AUD-001 = `DONE_STATIC_WITH_ERRATUM`，**禁止重跑**。
- 原文 [evidence/C/01-static-interface-audit.md](evidence/C/01-static-interface-audit.md) SHA `47de55b082f0654828e427f850e703d4e85953725b8c2d9c59ca4bde27275aba`，实测 `wc -l` = 103（有记录按 104 行阅读）。
- **第 96 行把仓库颜色 ONNX `9d41c7…` 误记为 R84。** 必须同时读 [evidence/C/ERRATA_STATIC_AUDIT_LINE96.md](evidence/C/ERRATA_STATIC_AUDIT_LINE96.md) 与 [evidence/C/ERRATUM_01M7KKBRVQJ1MWYQ8YEHQEYSMG.md](evidence/C/ERRATUM_01M7KKBRVQJ1MWYQ8YEHQEYSMG.md)。不要改写原文哈希。

## 离线几何与 mock（非 NPU / 非视频）

- 夹具包（不入本 Git 包，只登记）：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/preprocess_fixtures.tar.gz`，1,677,077 B，SHA256 `a8df601e060874357cad83f947abcd8a71361a2e7816811de0b9b6a4122746d9`。基于 TRAIN400 manifest `db2c85bd16fe777cd39bdff8d1fdf96f5e9ade4ce263bdc35c510becff8435e7`。21 个 double + 顺序前 21 个 single，**SU 0**。角色是 TRAIN 预处理对照，不是验证集、不是模型。
- C-IF-006：42/42 U8 逐像素与 PNG SHA 一致；F32 按 BGR NCHW u8/255，42/42 CHW SHA 全等，max ULP 0。非 ORT、非 NPU。第一次运行为 `NOT_RUN_DEPENDENCY`（缺 C++ OpenCV），**不是像素 FAIL**，历史保留。helper SHA `2906c185fc124ee76f905f9f208a418f6719b925b92eed91a330efe341560c09`。raw96 double 负控 21 张差异像素 9183–9216，仅记录。SU / 线上 layer 决策仍 OPEN。
- C-IF-007：对 `classify_color` 实际函数体做 mock InferEngine（无模型），**52 断言 PASS**。不是模型/NPU/runtime/video。

## 模型绑定（颜色）

- 仓库 `plate_color_finetune.onnx` `9d41c7dc…` 与 canonical `ecba573c…`、学生 `9c840305…` **字节不同**；`9d41` 的训练继承 **UNKNOWN**。
- 同名训练 tar 合同路径曾缺失，档案使用 `/workspace/cn_val_su/original_mixed_finetune_20261107.tar.gz`，记录 SHA `c1d86ea45b6037a9c3fa0a396b4d66716da65d26b59e91620993ca39194e4f10`。包内无训练器。tar 内 `infer_plate.py` 与仓库 `3d00d821…` 字节不同。
- canonical special-double：**先横拼再 resize96**。`source=='su'` 是数据集来源标签（在 crop 中提前返回、不拼），**不是线上运行时标志**。不得把预测层当成 source GT。运行时 layer 风险未闭合。
- **C-IF-004** 仍是「按模型契约选择预处理」的提案，`PENDING_PARENT_REVIEW_PROPOSAL_ONLY` / `ready_to_apply=false`。后来只批准并完成了离线像素对照（C-IF-005/006），**没有**批准把该契约设成产品默认或启用未过门候选。区分：可继续评审提案 ≠ 已准入改代码。

## 不要重跑的既有测试

| 项 | 原因 |
|---|---|
| R40 176 | 合成 InferEngine 跨版本位/SHA 比较，不是真实模型/NPU/视频 |
| R41 32 CPU ORT | 已有三模式逐字节、一模式仅阈值内的结论 |
| R46 43 fixture / 54 main | 格式工程合成证据，不是真实 NPU 视频 |
| R50 五类 NPU | 数值门失败，不得用同一失败产物同义再跑来「再试一次」 |
| C-AUD-001 | 已完成且有勘误 |
| C-IF-006 42 / C-IF-007 52 | 已完成；不要再派几何或 mock |
| 对 10.100.100.101 或 192.168.151.200 空转盲探 TCP/22 | 2026-11-17 已测：101 当时不可达；200 已加上且 ICMP 通但 **无 22 监听**。在未确认监听或未获得临时启动授权前不要重复空探 |

## 板端（2026-11-17 实测，不是今天）

用户于 2026-11-17 11:27 明确授权：通过串口修改一个 `151.x` 地址以便连接。这不是修复/重启/改生产文件的授权。

该次做法与结果：

- 机器 `level`（当时在线），COM3，115200/8N1，Open 前 DtrEnable=false、RtsEnable=false、Handshake=None；未故意脉冲/break/reset。
- 已有 `/opt #` 会话，未再登录，未把密码写入日志。
- 只读采集后，确认无冲突，执行 `ip addr add 192.168.151.200/24 dev eth0`（RAM，ADD_EXIT=0）。
- 保留原 `10.100.100.101/24`、MAC `00:18:f5:a0:a9:6e`、默认路由 `default via 10.100.100.1 dev eth0`。未写 `/etc` 或启动项。
- 用户机当时 `192.168.151.152/24`，未改 PC 网卡。
- 2026-11-17T11:36:45+08:00：对 `.200` ICMP 成功，邻居 MAC 一致；**TCP/22 失败**。串口 netstat 无 22 监听。上轮执行范围是不启动 sshd，因此未登录 SSH。
- 串口实测 ext4 `errors_count`：mmcblk0p13=0，**mmcblk0p14=2168**。这是新测量；历史 R48 也曾为 2168，两者不要混成一次。
- 回退命令已记录、当时工作正常故未自动执行：`ip addr del 192.168.151.200/24 dev eth0`。
- R44 **8/8 项目文件备份**不是系统镜像，不解除维修或恢复授权。

关于 SSH 服务：上轮执行范围是不启动 sshd，不要把这一点写成用户永久禁令。下一任若认为需要临时启动，应重新确认服务是否必要以及对应授权范围。

整理时补充、未在该交付目录独立落盘：2026-11-19 COM3 消失（Code 45）；当日 18:11–18:12 最后一次成功网络复核；此后用户机多数离线。**不要把 11 月 17 日的地址或串口状态说成 11 月 23 日仍然如此。** RAM 地址可能已丢失。

存储异常（p14 计数、无系统镜像）未修复；用户禁止修复/恢复/重启/生产修改。只读健康若再做，遇新 I/O error 应停止进一步负荷。

连通或临时地址成功 **不是发布通过**。
