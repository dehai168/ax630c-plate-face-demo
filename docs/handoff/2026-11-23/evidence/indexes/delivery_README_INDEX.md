# v2.5 delivery index — 2026-11-17

生成时间：2026-11-17T09:38:19+08:00（Asia/Shanghai (UTC+8)）。

本文件只提供三线目录与队列的路径引用，以及跨线依赖关系。各线只写自己的子目录，避免并发覆盖。

**A/B 的阶段方向以下文「父任务转述」为准，本索引不重验其产物，也不把路径或阶段交付写成发布门通过。** 各线只写自己的子目录；本更新未修改 A/ 或 B/ 下任何文件。

## 目录

- 工作流 A（OCR 识别模型）：[`A/`](A/)
- 工作流 B（五类颜色模型量化）：[`B/`](B/)
- 工作流 C（工程集成与板端验收）：[`C/`](C/)

## 各线队列与 C 线已落盘文件

- 总队列（仅路径与依赖）：[`NEXT_TASK_QUEUE.json`](NEXT_TASK_QUEUE.json)
- A 线队列：[`A/NEXT_TASK_QUEUE.json`](A/NEXT_TASK_QUEUE.json)
- B 线队列：[`B/NEXT_TASK_QUEUE.json`](B/NEXT_TASK_QUEUE.json)
- C 线队列：[`C/NEXT_TASK_QUEUE.json`](C/NEXT_TASK_QUEUE.json)
- C 线本批产物：[`C/ARTIFACT_INDEX.json`](C/ARTIFACT_INDEX.json)、[`C/BASELINE_COMPARISON.json`](C/BASELINE_COMPARISON.json)、[`C/SAFE_BOARD_SCOPE.json`](C/SAFE_BOARD_SCOPE.json)、[`C/STAGE_SUMMARY.md`](C/STAGE_SUMMARY.md)、[`C/HANDOFF/CLOUD_HANDOFF.md`](C/HANDOFF/CLOUD_HANDOFF.md)；本阶段新增 [`C/ERRATA_STATIC_AUDIT_LINE96.md`](C/ERRATA_STATIC_AUDIT_LINE96.md)、[`C/INTERFACE_CONSISTENCY_EVIDENCE.json`](C/INTERFACE_CONSISTENCY_EVIDENCE.json)、[`C/NEXT_EXPERIMENT_PROPOSAL.json`](C/NEXT_EXPERIMENT_PROPOSAL.json)、[`C/INTERFACE_CONSISTENCY_STAGE.md`](C/INTERFACE_CONSISTENCY_STAGE.md)；契约合并新增 [`C/CONTRACT_MERGE_STAGE.md`](C/CONTRACT_MERGE_STAGE.md)、[`C/MODEL_INPUT_BINDING_MATRIX.json`](C/MODEL_INPUT_BINDING_MATRIX.json)、[`C/COLOR_PREPROCESS_POLICY_PROPOSAL.json`](C/COLOR_PREPROCESS_POLICY_PROPOSAL.json)、[`C/CLOUD_NUMEL3_GUARD_REGISTRATION.json`](C/CLOUD_NUMEL3_GUARD_REGISTRATION.json)；夹具交接指针 [`C/PREPROCESS_FIXTURE_HANDOFF.md`](C/PREPROCESS_FIXTURE_HANDOFF.md)、修订合同 [`C/COLOR_PREPROCESS_PIXELCOMPARE_CONTRACT.json`](C/COLOR_PREPROCESS_PIXELCOMPARE_CONTRACT.json)；审计归档 [`C/AUDIT_EVIDENCE/`](C/AUDIT_EVIDENCE/)、IF006 第一次运行归档 [`C/IF006_EVIDENCE/`](C/IF006_EVIDENCE/)、执行报告 [`C/IF006_EVIDENCE/GEOMETRY_EXECUTED.md`](C/IF006_EVIDENCE/GEOMETRY_EXECUTED.md)；IF007 [`C/IF007_EVIDENCE/RESULT.md`](C/IF007_EVIDENCE/RESULT.md)；预检 [`C/BOARD_READONLY_PREFLIGHT_STAGE.md`](C/BOARD_READONLY_PREFLIGHT_STAGE.md)

## 跨线依赖（不把单线失败扩成停工）

- 发布 v2.5 必须同时满足 A 的同口径 OCR 保护、B 的原始 CAL64 门槛、C 的离线/NPU/板端完整视频/生产验收四级，且不得降低已确定标准。
- C 线独立分支上的运行时、格式、接口与离线集成工作不需要等 A、B 结束。
- C 线五类 NPU 数值验收与带新颜色产物的板端视频，依赖 B 产出通过原始 CAL64 门槛的候选；不得用已失败的同一产物同义重跑来“再试一次”。
- 若 C 线视频路径将替换识别器，还需 A 的同口径候选；在此之前，C 不得把软件或合成成绩写成生产识别通过。
- A、B 的模型工作不需要等待 C 的板端完整视频；也不得把编译通过或离线断言写成验收通过。
- 生产 v2.4 在三线任一未完成时保持不变、随时可回退。


## 发布状态（2026-11-17T09:46:25+08:00）

全部发布门仍未通过。板端完整视频未做。C 线云端静态补丁重放成功**不得**写成 NPU 实际推理通过或生产验收通过。生产 v2.4 保持可回退。

## 父任务转述的当前方向（非本登记重跑/重验）

- A：二阶段已交付。当前定向 R34–R44 板外 rec 生产身份预检；身份充分时做一次冻结 600×6 软件对照。详见 [`A/NEXT_TASK_QUEUE.json`](A/NEXT_TASK_QUEUE.json)。不据此宣称 OCR 发布门已过。
- B：首批已交付。当前已有学生 ONNX 64CPU 漂移与量化分离工作（bound 2CPU / 2GiB / 120s）。详见 [`B/NEXT_TASK_QUEUE.json`](B/NEXT_TASK_QUEUE.json)。不据此宣称原始 CAL64 门槛已过。
- C：见下一节。

## C 线状态更新（2026-11-17T09:46:25+08:00）

- 交接包仍为 [`C/HANDOFF/`](C/HANDOFF/)。队列：[`C/NEXT_TASK_QUEUE.json`](C/NEXT_TASK_QUEUE.json)。
- remote：`https://github.com/dehai168/ax630c-plate-face-demo`
- 独立分支：`cursor/v25-c-integration-audit-50d6`
- base：`08eb9d6baa8e3c611dc8f3230df80fa1dcdecd07`；final：`3114764f0ee64ea576cc008a56748a0520194e81`；v2.4 tag peeled：`f848b7d5fe1470a91c684f4ba5b115451b675ea3`；**main 未变**。
- 云端报告：静态无漂移补丁重放完成，未编译、未推理；handoff 六源文件最终 SHA 均匹配。这是父任务转述的云端结果，**不是本地对 commit 的独立重哈希，不能伪称本机看过该 commit**。
- 范围限制：原 main 祖先已有 `1711ff5` ONNX 变更，因此只是这六源文件与 v2.4 归档对齐，**不是整个仓库等于 v2.4 归档**。
- 下一云端阶段已启动：只读接口/开关/视频 NPU 数据流审计（不编译、不推理、不改代码）。登记为 `C-AUD-001`，本登记不新开实验。
- 静态成功 ≠ NPU 通过 ≠ 板端完整视频通过 ≠ 生产验收通过。


## A 线状态更新（2026-11-17T09:52:20+08:00）

本段仅为 A 线追加状态引用，不修改上文 B/C 段落，也不重验 B/C 产物，不宣称发布门通过。

- 第四阶段已交付：在冻结 val600 accepted locked probs 上，测量本机 CPU 隔离的 greedy vs 锁定 Prefix 解码成本。决策仅为主测量可复现性接受。详见 [`A/stage4_isolated_decoder_latency_20261117/STAGE4_SUMMARY.md`](A/stage4_isolated_decoder_latency_20261117/STAGE4_SUMMARY.md)、队列 [`A/NEXT_TASK_QUEUE.json`](A/NEXT_TASK_QUEUE.json)。
- 不是 NPU/板端/TTA 六次 forward/整帧端到端，也不是模型或发布接受。生产 v2.4 × val600 × Prefix+TTA 软件格仍为 UNKNOWN（缺官方浮点对应链，不再反复身份扫描）。TTA 六次 forward 仍 UNMEASURED。


## A 线状态更新（2026-11-17T09:58:20+08:00）

本段仅为 A 线追加状态引用，不修改上文 B/C 段落，也不重验 B/C 产物，不宣称发布门通过。

- 第五阶段已交付：在 accepted ONNX 与冻结 val600 上，测量本机 x86 ORT CPU 原视角 vs 锁定六视角 TTA 的隔离成本。决策仅为主测量可复现性接受。详见 [`A/stage5_tta_isolated_cpu_latency_20261117/STAGE5_SUMMARY.md`](A/stage5_tta_isolated_cpu_latency_20261117/STAGE5_SUMMARY.md)、队列 [`A/NEXT_TASK_QUEUE.json`](A/NEXT_TASK_QUEUE.json)。
- 不是 NPU/板端/全帧，也不是理论 6 倍，也不是模型或发布接受。A 线无需新模型的独立可自动执行项已穷尽。生产浮点基线格仍 UNKNOWN（不再反复身份扫描）。板端/NPU 时延仍交 C。B/C 继续。

## C 线状态更新（2026-11-17T09:56:30+08:00）

本段仅为 C 线追加接口一致性取证，不修改上文 A/B 段落，也不重验 A/B 产物，不宣称发布门通过，也不把默认路径未证实 bug 写成 v2.5 全部接口已闭合。

- 合同与勘误：只读云端勘误文件；不改原 `01-static-interface-audit.md`。旧报告 SHA 保持 `47de55b082f0654828e427f850e703d4e85953725b8c2d9c59ca4bde27275aba`（第 96 行误把仓库 `9d41c7` 写成 R84，见 [`C/ERRATA_STATIC_AUDIT_LINE96.md`](C/ERRATA_STATIC_AUDIT_LINE96.md)）。不引用原错误结论。
- 训练包：合同路径不存在；同名产物 `/workspace/cn_val_su/original_mixed_finetune_20261107.tar.gz` SHA 匹配 README 预期 `c1d86ea45b6037a9c3fa0a396b4d66716da65d26b59e91620993ca39194e4f10`。包内无训练器。颜色 ONNX 为 `9d41c7…`，与 R84 `9c840…` / canonical `ecba573…` 字节不同；继承 UNKNOWN。包内 `infer_plate.py` 为 `6f224705…`，不是仓库 demo `3d00d821…`。双层颜色训练输入仍未闭合；改 `classify_color5` 输入仍不可执行。
- 旧三类打包：归档 `Tensor::size()` 走逻辑 shape 乘积而非 `nSize`。v2.4 归档 `plate_cls_96x96.onnx`（`ce20ab62…`，即 color_original）图输出 `[1,3]`。历史 R36 npu1 30 条 color3 均为 `[1,3]` / 12 字节。`size()==3` 提案改为待父方审阅后可执行，本阶段不改代码。npu2 仍未直接观测。
- 产物：[`C/INTERFACE_CONSISTENCY_STAGE.md`](C/INTERFACE_CONSISTENCY_STAGE.md)、[`C/INTERFACE_CONSISTENCY_EVIDENCE.json`](C/INTERFACE_CONSISTENCY_EVIDENCE.json)、[`C/NEXT_EXPERIMENT_PROPOSAL.json`](C/NEXT_EXPERIMENT_PROPOSAL.json)。队列 [`C/NEXT_TASK_QUEUE.json`](C/NEXT_TASK_QUEUE.json)。
- 发布门与板端完整视频状态不变。生产 v2.4 保持可回退。

## C 线状态更新（2026-11-17T10:04:03+08:00）

本段仅为 C 线追加契约合并与云端 ==3 登记，不修改上文 A/B 段落，也不重验 A/B 产物，不宣称发布门通过。

- 云端旧槽守卫（父转述，非本地重哈希，非本登记启动）：独立分支 `cursor/v25-c-integration-audit-50d6` 新提交 `2241e28c110f8a10b282619459743964e8b79d4e`（原 `3114764`）。`src/plate_rec.cpp` SHA `2f1eedd78dd5705e4400ede44027098224ca1cf5239bc0d3e98dfc3cb45857a0`，第 204 行唯一 `>=3`→`==3`；其余五源 SHA 不变；`main` 仍为 `08eb9d6`。stdlib 谓词小测试 0.214 秒，0/1/2 拒、3 接受、4/5/6/12 拒，fails=0。接受范围为 **accept_no_release**。不是完整 classify_color / runtime / NPU2 / 视频通过。未 merge、未部署、未开 PR。见 [`C/CLOUD_NUMEL3_GUARD_REGISTRATION.json`](C/CLOUD_NUMEL3_GUARD_REGISTRATION.json)。
- 已消费 B14（不重扫）：[`B/B14/REPORT.md`](B/B14/REPORT.md)、[`B/B14/CROSSLINE_DOUBLE_COLOR_EVIDENCE.json`](B/B14/CROSSLINE_DOUBLE_COLOR_EVIDENCE.json)。模型输入矩阵与完整十字段提案：[`C/MODEL_INPUT_BINDING_MATRIX.json`](C/MODEL_INPUT_BINDING_MATRIX.json)、[`C/COLOR_PREPROCESS_POLICY_PROPOSAL.json`](C/COLOR_PREPROCESS_POLICY_PROPOSAL.json)、综述 [`C/CONTRACT_MERGE_STAGE.md`](C/CONTRACT_MERGE_STAGE.md)。
- canonical `ecba573…` 与学生 `9c840…` 的 special-double 拼接已证；`source=='su'` 例外不拼；仓库 `9d41c7` 训练仍 UNKNOWN；字节不同与继承 UNKNOWN 分开。当前 C++ raw rectified 与已证训练口径不符，是面向未来通过 B 门的 canonical 派生候选的真正集成缺项。提案 `ready_to_apply=false`，待父方按完整十字段审阅。部署归档运行时无 source 字段。不启用失败 R84。不把默认路径或 ==3 写成全部接口闭合。生产 v2.4 保持可回退。

## C 线状态更新（2026-11-17T10:10:49+08:00）

本段仅为 C 线追加离线预处理夹具交接，不修改上文 A/B 段落，不重验 A/B，不宣称发布门通过，不覆盖此前政策提案，不改独立分支提交 2241e28。

- 新十字段合同（仅离线逐像素对照）：[`C/COLOR_PREPROCESS_PIXELCOMPARE_CONTRACT.json`](C/COLOR_PREPROCESS_PIXELCOMPARE_CONTRACT.json)。上一份 [`C/COLOR_PREPROCESS_POLICY_PROPOSAL.json`](C/COLOR_PREPROCESS_POLICY_PROPOSAL.json) 保留未覆盖。
- 夹具包（供父方派 Cloud 读取，本登记不启动调用）：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/`，说明 `/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/README.md`，压缩包 `/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/preprocess_fixtures.tar.gz` SHA256 `a8df601e060874357cad83f947abcd8a71361a2e7816811de0b9b6a4122746d9`（1677077 B）。指针 [`C/PREPROCESS_FIXTURE_HANDOFF.md`](C/PREPROCESS_FIXTURE_HANDOFF.md)。
- 读图前冻结：TRAIN400 `db2c85bd…` 全部 21 double + 顺序前 21 single。该 manifest 无 SU，未补样。42/42 源 SHA 匹配且可解码；rectify 中间值已绑定。不宣称 Python/C++ 一致（未测 C++）。
- 不发明运行时 source 字段。线上层误判风险保留。生产 v2.4 保持可回退。

## C 线状态更新（2026-11-17T10:16:00+08:00）

本段只纠正队列状态，不修改上文 A/B 段落，不复制 A/B 陈旧结论，不重验 A/B，不开新实验，不宣布项目空闲。

- **C-AUD-001 = `DONE_STATIC_WITH_ERRATUM`，禁止重跑。** 只读 dataflow 审计已完成。原文归档 [`C/AUDIT_EVIDENCE/01-static-interface-audit.md`](C/AUDIT_EVIDENCE/01-static-interface-audit.md)，SHA256 `47de55b082f0654828e427f850e703d4e85953725b8c2d9c59ca4bde27275aba`，实测 `wc -l` = 103（父方按 104 行阅读，与既有勘误说明一致）。第 96 行误把仓库 9d41 称作 R84，必须同时阅读勘误 [`C/AUDIT_EVIDENCE/ERRATUM_01M7KKBRVQJ1MWYQ8YEHQEYSMG.md`](C/AUDIT_EVIDENCE/ERRATUM_01M7KKBRVQJ1MWYQ8YEHQEYSMG.md)。这不是 NPU 或视频验收。
- **C-IF-006 不是尚未启动，也不能宣布 idle。** 第一次 Cloud 运行为 `NOT_RUN_DEPENDENCY`（缺 C++ OpenCV，前向之前停止），**不是**像素 FAIL；见 [`C/IF006_EVIDENCE/FIRST_RUN_REPORT.md`](C/IF006_EVIDENCE/FIRST_RUN_REPORT.md)。父已在同一 cloudagent `bc-e418510a-70f5-5a35-8635-d476608950d6` 的 run 6 进行环境准备（官方签名 apt：core/imgproc/imgcodecs），随后做同一 42 TRAIN helper 对照。环境准备 = `IN_PROGRESS`；像素测量 = `NOT_STARTED_WAITING_ENV`。禁止再发新的 geometry 任务。代码仍为 2241e28。只限 CPU helper，不是 full build / 模型 / NPU。
- 项目因 C 线运行而 **IN_PROGRESS**。A/B 为外部依赖，请看其当前文件：[`A/NEXT_TASK_QUEUE.json`](A/NEXT_TASK_QUEUE.json)、[`A/STAGE_SUMMARY.md`](A/STAGE_SUMMARY.md)、[`B/NEXT_TASK_QUEUE.json`](B/NEXT_TASK_QUEUE.json)、[`B/B17/REPORT.md`](B/B17/REPORT.md)。A 此前已报告生产浮点链缺失与软件成本测量完成；B17 已记录一次 CLI 诊断 0/64 无输出。此处不重述为新结果，也不覆盖根上已有 B 字段。

## C 线状态更新（2026-11-17T10:21:18+08:00）

本段只登记 Cloud run 6 的离线几何结果（父转述，非本机重跑），不修改上文 A/B 段落，不催用户补 A/B，不开新实验，不重复派发 geometry。

- **C-IF-006 = `DONE_OFFLINE_GEOMETRY_42_U8_F32_NON_NPU`。** 报告副本 [`C/IF006_EVIDENCE/GEOMETRY_EXECUTED.md`](C/IF006_EVIDENCE/GEOMETRY_EXECUTED.md)，SHA256 `c55f02a163dacbb658290555605ee2d040ec7206c5499acab419fa4708c1acfe`（源资产 `01M7KMTBFNEHPNVB8W2F86R81Q.md`）。父已向用户交付。
- 42/42 U8 逐像素与 expected PNG 文件 SHA 一致（double 21 / single 21 / SU 0）。F32 按现有 BGR NCHW u8/255，42/42 与 expected CHW SHA 全等，max ULP 0。并非 ORT linked，不是 NPU。云端 OpenCV 4.6.0 vs 夹具生成 5.0.0，未改插值。helper SHA `2906c185…` 未变。apt 56 包、下载 39.9 MB、磁盘 +155803648 B、安装约 5s；harness 编译 1.020s、对照 32.721s。
- 第一次 `NOT_RUN_DEPENDENCY` 历史保留，不写成像素 FAIL 后 retry。代码仍为干净的 `2241e28`。raw96 double 负控 21 张差异像素 9183–9216，仅记录、无调参。SU / 在线 layer 决策仍 OPEN。
- 下一专项 **C-IF-007** 由父在同一 cloud 启动：旧三类 `classify_color` 实际函数主体（mock InferEngine，不模型）误配置与 HSV 回退，不同于已过谓词测试。本登记不 dispatch。不完整应用 build、不重跑 176、不 NPU。禁止再发 geometry 任务。
- 项目仍为 IN_PROGRESS，队列不是 idle。A/B 维持外部缺口，见其当前文件，不在此重复催促。

## C 线状态更新（2026-11-17T10:26:31+08:00）

本段登记 IF007 mock 归档与只读连接预检，不修改上文 A/B 段落，不把 mock/Python/x86 写成 NPU 实际，不向用户重复索取已知 COM3 或密码。

- **C-IF-007 = `DONE_OFFLINE_ACTUAL_FUNCTION_MOCK_ONLY`。** 归档 [`C/IF007_EVIDENCE/RESULT.md`](C/IF007_EVIDENCE/RESULT.md) SHA256 `a98a671fa927fe8d6164954d3ce0843e694c7dc39d2762011fd7af6c54a9e5d9`（合同 `a36361a0…`）。52 断言 PASS。不是模型/NPU/runtime/video。2241e28 六源未改。父已向用户交付。
- **C-PF-001 预检完成：** 合同先于 machine 命令冻结。`level` 在线。2026-11-17T10:26:04+08:00 当前仅 COM3，PnP OK，Prolific 与 R34 同一 InstanceId。无已知桥接进程名；白名单目录存在但无匹配脚本。未打开串口、未发字节、未改 DTR/RTS。历史 115200+newline 方法本轮禁用。历史能连 ≠ 当前能连。见 [`C/BOARD_READONLY_PREFLIGHT_STAGE.md`](C/BOARD_READONLY_PREFLIGHT_STAGE.md)。
- **C-PF-002 受阻：** 无已证明、不切换 DTR/RTS、已 authenticated 的只读通道，故不读取 mount / errors_count，不把历史 2168 当作本次重测。下一项依赖该通道（若需登录，父按 vault 规程处理）。
- 项目仍为 IN_PROGRESS，队列非整体 idle。A/B 维持外部缺口，见其文件，不催用户。

## C 线状态更新（2026-11-17T10:43:35+08:00）

本段登记独立 C-PF-SSH 合同与单目标连通预检，取消把串口物理确认当作全线阻塞，不修改上文 A/B 段落，不打开串口，不启用 sshd，不重跑 42/52，不把历史 2168 当作本次实测。

- **C-PF-SSH-001 = `BLOCKED_NO_REACHABLE_EXISTING_SSH`。** 合同 [`C/BOARD_SSH_READONLY_CONTRACT.json`](C/BOARD_SSH_READONLY_CONTRACT.json) SHA256 `e0ad0d7b539399e23513db9b1ad01a59343a25d9a2391ada9a87e41d59ed4ccb`。结果 [`C/BOARD_SSH_READONLY_RESULT.json`](C/BOARD_SSH_READONLY_RESULT.json) SHA256 `c2676638c6e655a94b1ffa38934e136a3ac31e6c8c8c2ec08a65ec790c716208`。说明 [`C/BOARD_SSH_READONLY_STAGE.md`](C/BOARD_SSH_READONLY_STAGE.md)。
- 端点来源为既有档案，不是扫描：R43（2026-11-13T15:51:52–15:55:13+08:00）最后记录板卡 eth0 `10.100.100.101/24`。用户机 `level` 仍为以太网 `192.168.151.152/24`。未把 10.30.25.73 / 10.30.25.10 / 192.168.151.152 当作板卡。临时历史地址 192.168.151.200 未探测。R48 新 boot 后未再确认该 IP。
- 2026-11-17T10:41:13+08:00 在 machine `level`：已有 `ssh.exe`；known_hosts 无该主机；ICMP 失败；TCP/22 失败。未尝试登录，未测 hostname/uname/uptime/mounts/errors_count/df。未改 known_hosts，未关主机校验。
- 串口本轮未打开。C 的下一依赖是从 level 可达的既有 SSH（若现地址已变，需非扫描来源的当前 IP/端口），不是串口物理确认。
- 本 worker 已结束：root `active_work` 已清空，`queue_is_idle` true，`project_status` = `BLOCKED_EXTERNAL_DEPENDENCIES`（实际 blocked，不是 running）。A/B 根字段未覆盖。

## C 线状态更新（2026-11-17T11:38:26+08:00）

本段登记用户 11:27 授权后的串口临时 151 地址添加与 SSH 预检，不修改上文 A/B 段落，不把连通写成发布通过，不重跑 42/52。

- **C-PF-SSH-002 = `COMPLETED_TEMP_ADDR_ADDED_NO_SSH_LISTENER`。** 合同 [`C/BOARD_SERIAL_TEMP_ADDR_SSH_CONTRACT.json`](C/BOARD_SERIAL_TEMP_ADDR_SSH_CONTRACT.json) SHA256 `7612b6311b5848575dc04062e3f551909a5e1da50e2650394e060786327dcf97`。结果 [`C/BOARD_SERIAL_TEMP_ADDR_SSH_RESULT.json`](C/BOARD_SERIAL_TEMP_ADDR_SSH_RESULT.json) SHA256 `430d393a3ebb3cd603084bbfbc192438bf9a7bf19d76ee6144034bfc115c0664`。说明 [`C/BOARD_SERIAL_TEMP_ADDR_SSH_STAGE.md`](C/BOARD_SERIAL_TEMP_ADDR_SSH_STAGE.md)。
- 实际新 IP：`192.168.151.200/24` 已用 `ip addr add` 加在 eth0（RAM，ADD_EXIT=0）。原 IP `10.100.100.101/24`、MAC `00:18:f5:a0:a9:6e`、默认路由 `via 10.100.100.1` 保留。回退命令已记录未执行：`ip addr del 192.168.151.200/24 dev eth0`。
- SSH 认证未完成：地址在 2026-11-17T11:36:45+08:00 可 ICMP/邻居可达，TCP/22 无监听。未启动 sshd。
- 只读健康经已认证串口（非 SSH）实测：p13 `errors_count`=0，**p14 `errors_count`=2168**（当前测量，虽与历史 2168 相同但不是复用旧数）。`/opt` 与 freq_rw 在 mmcblk0p14，scare_rw 在 mmcblk0p13。
- 板端持久文件写入：无。PC 网卡未改。本 worker 已结束，root `active_work` 已清空，状态为实际 blocked/idle，不是 running。
