# AX630C 车牌人脸 v2.5 交接包（2026-11-23）

生成时间：2026-11-23T20:15:44+08:00（Asia/Shanghai（UTC+8））

本包供不依赖原聊天、不熟悉前文的接手开发者使用。整理阶段未向 Git 推送，也未 clone 或修改应用仓库、未接触用户机或板卡，未改 `delivery_20261117` 的 A/B/C 队列状态。提交阶段再核验远端，且不可覆盖他人已提交内容。

**当前发布状态：v2.5 NO-GO。生产 v2.4 必须保持可回退，不得当作已发布。** 软件对照、离线几何、函数 mock、串口临时地址、ICMP 可达，都不是 NPU / 完整视频 / 生产验收。

## 先读这 5 份

1. 本文件（入口、第一天清单、禁止项）
2. [PROJECT_STATE.md](PROJECT_STATE.md)（总状态与仓库身份）
3. [NEXT_TASKS.md](NEXT_TASKS.md)（下一步、授权边界、不要重复的实验）
4. 按职责再读：[A_OCR.md](A_OCR.md)、[B_COLOR.md](B_COLOR.md)、[C_INTEGRATION_BOARD.md](C_INTEGRATION_BOARD.md)
5. 资产与路径：[DATA_MODELS_ENV.md](DATA_MODELS_ENV.md)；证据副本：[EVIDENCE_INDEX.md](EVIDENCE_INDEX.md)

其他：[EXPERIMENT_HISTORY.md](EXPERIMENT_HISTORY.md)、[APPENDIX_OTHER_WORK.md](APPENDIX_OTHER_WORK.md)（海外车牌人脸、Label Studio、特种牌采集模板等；未在本应用仓库实现的，不得宣称已实现）、[PRIVACY_AND_SANITIZATION.md](PRIVACY_AND_SANITIZATION.md)、[MANIFEST.json](MANIFEST.json)、[scripts/check_handoff.py](scripts/check_handoff.py)。

精选原文副本在 [evidence/](evidence/)。关键结论已写入正文。`evidence/` 内的历史报告保留当时用词，以本目录新写正文为准理解当前口径。

## 第一天任务清单

1. 通读本文件、`PROJECT_STATE.md`、`NEXT_TASKS.md`。
2. 用标准 git 只读核验远端（本包记录的是最后已知提交，不保证仍等于当前远端）：
   - 仓库：`https://github.com/dehai168/ax630c-plate-face-demo`
   - C 分支最后已知：`cursor/v25-c-integration-audit-50d6` @ `2241e28c110f8a10b282619459743964e8b79d4e`
   - `main` 最后已知：`08eb9d6`（完整哈希以提交阶段核验为准；档案中常记作 `08eb9d6…`）
   - 若远端与上述不一致：先停并记录差异，不要假设本包描述仍完全适用于当前树。
   - 提交本包时基于最新远端创建提交，不可 rebase/force 覆盖他人已推送的历史。
   - 2026-11-23 提交前已用 `git ls-remote` 核到：`main` 为 `08eb9d6baa8e3c611dc8f3230df80fa1dcdecd07`，C 分支为 `2241e28c110f8a10b282619459743964e8b79d4e`，与上面最后已知提交一致。记录见 [GIT_SUBMISSION.md](GIT_SUBMISSION.md)。
3. 工作范围限定为独立非生产分支；不修改生产 v2.4 树；不要把 `2241e28` 写成已合并部署（档案记载尚未合并部署）。
4. 若要动板卡或 SSH：先读 `C_INTEGRATION_BOARD.md` 的时间口径与授权边界。不要把 2026-11-17 的地址/串口观察当成今天的状态。不要再空转盲探 TCP/22。
5. 跑本包自检（只检查文档相对链接与清单哈希，无外部副作用）：
   `python3 scripts/check_handoff.py`
6. 对照 `NEXT_TASKS.md` 再开实验。当前未获新实验准入的项，需新的十字段提案与相应授权，再开始训练或其他新实验。

## 继续前检查

- 队列实况以档案 `/workspace/cn_val_su/v25/delivery_20261117/NEXT_TASK_QUEUE.json` 为准（本包有副本）。本次交接没有更新该队列。
- 整理日（2026-11-23）在 `delivery_20261117` 与 `cn_val_su/v25` 文本报告中，未发现 2026-11-17 12:00 之后的新落盘实验。11 月 19 日 COM3/网络、11 月 23 日巡检情况见 `PROJECT_STATE.md` 中「整理时补充、未在该交付目录独立落盘」的说明，不要写成今日实测。
- 没有正在运行的本项目研发任务。发布仍为 NO-GO。

## 禁止项（摘要）

用户硬限制（未经新的明确用户授权不得做）：

- 修复 / 恢复 / 刷写 / 重启 / 格式化 / fsck / 改生产文件或生产模型
- 对外联系，或把 v2.5 当作已发布

关于训练：当前未获新实验准入，需新的十字段提案与相应授权。这不是永远禁止训练。

上轮执行范围或流程约定（不要写成用户永久禁令，也不要擅自扩大）：

- 上轮被要求不启动 sshd。下一任若需临时启动 SSH 服务，必须重新确认授权范围。
- 不降低已冻结门禁（OCR 已锁定 Prefix/TTA 与 val600 口径；颜色原门 Max≤0.03、MAE≤0.005、argmax 64/64）。
- 不把已失败的 R84 设为产品默认。
- 不为列哈希而递归碰 `private_root`。
- 不把密码、私钥、token、license、`private_root` 激活材料写入 Git。

已有用户授权（作为历史授权记录保留；是否仍适用于今天的硬件状态需重确认）：

- 2026-11-17 11:27 用户明确授权：通过串口修改一个 `192.168.151.x` 临时地址以便连接。该次已添加 RAM 地址，未写持久配置，未重启。原密码曾经提供过，但不得写入本交接文件。

## 各线外部依赖（精确）

- **A**：生产 rec `728d8f20…` 的官方 float/export 对应链仍然缺失；生产 val600 + 锁定 Prefix+TTA 软件格保持 UNKNOWN。不要再做身份扫描或 logits 重哈希。
- **B**：需要官方支持的 V8 float/ONNX 运行说明或可支持的校正；B17 唯一公开 CLI 已失败（0/64），不得重复或 patch 私有 API。现场 preobserver 捕获仍是单独缺口。原 CAL64 拒绝不变。
- **C**：工程集成不必等待 A/B 结束。五类 NPU、以及带新颜色产物的板端视频，依赖已经通过原 CAL64 门的候选。SSH 只读健康依赖板上已经在监听的 SSH，或经重新确认的临时启动授权。C-IF-004 仍是待审提案（应用侧），不要当成已批准改默认路径。

## 本包不含什么

- 全量模型、数据集、系统镜像、夹具压缩包本体、v2.4 源/板端 tar（路径与已知哈希见 `DATA_MODELS_ENV.md`）
- 聊天全文、密码、SSH 私钥、cookies、`.env`、付款与个人联系方式
- 对远端 Git 的现场核验结果（须在提交阶段用标准 git 完成）

## 时间与可信度

正文凡写「实测」都会带时间。最后一次在本交付档案中核到的板端只读/网络实测是 **2026-11-17**（Asia/Shanghai（UTC+8））。不要把旧观测说成 2026-11-23 的当前状态。
