# 附录：相关但并非本仓库已实现的工作

本附录只记录**可证的已知进度**。这些内容**不得**说成已经在 `ax630c-plate-face-demo` 生产或 v2.5 分支里实现，除非另有该仓库内的证据。

## 1. 特种车牌采集与 Label Studio

- 工作项草案：[evidence/docs/special_plate_data_plan.md](evidence/docs/special_plate_data_plan.md)（v0.1，2026-10-10），含 5 类颜色与特种牌采集/标注建议。建议工具写的是 Label Studio 或 CVAT，并有「模板素材」「标注工具模板」工作项，属于计划，不是已交付的平台配置本身。
- 评估报告：[evidence/docs/cn_val_su_eval_report.md](evidence/docs/cn_val_su_eval_report.md)（2026-10-15）。记载 Label Studio 项目 7 `cn_plate_val_su`，300 个任务，导出 `cn_val_su_export.json` SHA256 `abec3c9e94f97196a93f5ad86a1f417d3a82c812cf78cc20431daa836f6e0bf1`（当时 PC/服务器/box 三处一致）。
- 训练过程记录：[evidence/docs/special_train_report.md](evidence/docs/special_train_report.md)。多次写明疑似错标只挂审计旗、**未改 Label Studio / 正式 GT / 分母**。这是中国特种牌识别实验记录，与已接受 OCR 软件基线相关，但不是海外项目，也不是「模板任务已在本 repo 落地」的证明。

## 2. 海外车牌检测识别 + 人脸（车载 IPC）

- 方案 v1.0（记载为 2026-10-12 已批准、当时「未建 GitHub 项目」）：[evidence/docs/overseas_anpr_face_plan.md](evidence/docs/overseas_anpr_face_plan.md)。
- 独立工作区：`/workspace/ipc_overseas_anpr_face/`。README 副本：[evidence/docs/ipc_overseas_anpr_face_README.md](evidence/docs/ipc_overseas_anpr_face_README.md)。自述状态为 P0 里程碑 2（AXEngine 后端等），**板上验证待做**。这是另一套代码树，不要与 `ax630c-plate-face-demo` 的 v2.5 发布状态混写。

## 3. 「用户算法 / 模板任务」

本整理在交付目录中没有找到一份名为「模板任务已完成并合入本 repo」的验收记录。可见材料是：特种牌工作项里的标注模板计划、海外方案里的转换脚本模板设想，以及箱内其他互不替代的工程目录（例如独立海外代码树）。若后续要推进这些任务，应依据各自目录与授权单独立项，并写新的十字段提案。

## 4. 不要升级的说法

- 不得说海外算法已在生产 v2.4/v2.5 发布。
- 不得说 Label Studio 配置模板已作为本 Git 仓库的正式交付物（本包只收录了计划与评估报告）。
- 不得用附录进度替代 A/B/C 的验收门。
