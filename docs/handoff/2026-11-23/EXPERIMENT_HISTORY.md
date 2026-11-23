# 实验历史（早期到当前）

完整标题索引（R1–R85，根据各轮 `REPORT.md` 首行与 `summary.json` 状态字段生成，不是新实验）：[evidence/indexes/optimization_rounds_title_index.tsv](evidence/indexes/optimization_rounds_title_index.tsv)。本整理是有限遍历，没有读每个文件。

## 主线如何走到现在

1. 特种牌与五类颜色数据采集规范、Label Studio 项目与软件微调，形成已接受 OCR 软件基线与颜色 canonical float（详见 A/B 正文与附录）。
2. **R31** 扩容训练门通过后，**R32** 一次外部软件评估拒绝该候选并冻结，禁止继续 held 或降门槛。
3. 板端只读与备份：**R34** 串口只读；**R36** 旧三类真实 NPU TRAIN 证明；**R37** 采集中断、raw 未恢复；**R38** 完整抓包传输 NO-GO；**R44** 8 项项目文件备份（不是系统镜像）；**R47** 系统备份准备完成但无系统镜像；**R48** 重启后只读，文件系统异常仍在；**R49** 复查未见新增 ext4 错误，不能认定已修复。
4. 集成：**R40** 176 离线合成集成；**R41** 32 组 CPU ORT；**R45/R46** 真 main + 43 fixture / 54 main 格式闭合，发布仍 NO-GO。
5. **R50** 五类真实 AX IO 闭合但数值门失败。
6. 颜色量化：**R69/R79/R82/R84** 官方路径候选均未过原 CAL64 门；**R84** 为当前最好但仍拒绝的终端记录。
7. 2026-11-17 交付：A 完成软件时延隔离测量；B11–B17 诊断至公开 CLI 失败即停；C 完成静态审计勘误、`==3` 候选登记、42/52 离线结果登记，以及授权下的串口临时 151 地址（无 SSH 监听）。

## 关键轮次标题

| 轮次 | 报告标题（档案原文首行） |
|---|---|
| R31 | # Round31：单次真实扩容 original20 全识别器训练——TRAIN门全部通过，等待parent审阅 |
| R32 | # Round32：固定R31候选一次外部软件评估——拒绝；未继续held，未发布 |
| R34 | # Round34续：已实际取得shell并完成只读路径/100源SHA preflight |
| R36 | # Round36 — recovered SDK and REAL native TRAIN backend proof |
| R37 | # LATEST: recovered access; original board capture directory absent; durable raw package incomplete |
| R38 | # Round38 — actual bounded persistence/transport feasibility: NO-GO for full capture |
| R40 | # Round40：五类颜色真实归档源集成离线核验（非生产验收） |
| R41 | # Round41 恢复完成：五类产物绑定与真实 C++ ORT 黄金参考 |
| R44 | # Round44 — 已完成原授权 8 项项目文件备份 |
| R45 | # Round45：真实归档视频入口离线集成核验 — 发布仍 NO-GO |
| R46 | # Round46：有来源的已批准格式闭合及默认strict实测 — 仍NO-GO |
| R47 | # Round47：系统级备份准备（已实际推进，尚未取得系统镜像） |
| R48 | # Round48：重启后只读核查 |
| R49 | # Round49复查完成：空间增加，未见新增ext4错误，但不能认定已修复 |
| R50 | # Round50：真实 AX 五类 IO 已核验，数值门禁仍失败（NO-GO） |
| R69 | # R69 — 校准64的官方逐层机制诊断 |
| R79 | # R79：唯一官方 EasyQuant 再尝试（10GiB/900s）——构建与激活搜索完成，CAL64 未过原终端门 |
| R82 | # R82 Report: unique official-grid QAT distillation, then official MinMax |
| R84 | # R84 Report: unique official MinMax of the already-exported student, then CAL64 |
| R85 | # R85 Report: residual comparison, R82 implementation audit, remaining-path survey |

## 仍为 UNKNOWN 的代表项

- 生产 rec `728d8f20` 的官方 float/export 链，以及该权重在冻结 val600+Prefix+TTA 上的软件格
- 仓库颜色 `9d41c7` 的训练继承（与 `ecba`/`9c840` 字节不同）
- 线上是否以及如何使用 special-double 拼接（无 source 运行时字段）
- 五类 NPU 数值在「已过原门的新候选」上的结果（当前没有这样的候选）
- 板端完整真实视频、生产时延/稳定性
- 2026-11-17 之后的板卡网络/串口/SSH 现状

## 接受范围 vs 失败范围（不要升级）

- 接受（有界）：已接受 OCR 软件模型与锁定解码/TTA；val600 554/600 文本一致；x86 时延测量可复现；颜色公式审计与 B14 字节复现（有明确限制）；C 离线 176/32/43/54/42/52；静态审计（含勘误）；2241e28 仅作为未部署候选；11-17 临时 RAM 地址添加成功但无 SSH。
- 失败/拒绝：R31；R69/R79/R84 CAL64；R50 五类 NPU 数值；R37/R38 完整采集传输；把 R44 八项备份当成系统备份。
- 不得把编译通过、check1、mock、ICMP、离线像素全等写成发布通过。
