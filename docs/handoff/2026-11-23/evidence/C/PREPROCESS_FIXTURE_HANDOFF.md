# C 预处理夹具交接（2026-11-17T10:10:49+08:00，Asia/Shanghai (UTC+8)）

本阶段只准备供 Cloud 使用的最小 TRAIN 夹具包，并修订完整十字段合同。**未改代码、未编译 repo、未跑 C++、未上板、未改 2241e28、未启用候选、未设 sidecar 默认、未调用 Cloud。** 不宣称 Python/C++ 一致。不覆盖上一份政策提案 `COLOR_PREPROCESS_POLICY_PROPOSAL.json`。

## 父方请读

- 合同（新，不覆盖旧提案）：[`COLOR_PREPROCESS_PIXELCOMPARE_CONTRACT.json`](COLOR_PREPROCESS_PIXELCOMPARE_CONTRACT.json)
- 包目录：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/`
- 包说明：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/README.md`
- 压缩包：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/preprocess_fixtures.tar.gz`（1677077 B，SHA256 `a8df601e060874357cad83f947abcd8a71361a2e7816811de0b9b6a4122746d9`）
- 成员清单：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/ARCHIVE_MANIFEST.json`、`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/MEMBER_SHA256SUMS.txt`
- 选择冻结（读图前）：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/FROZEN_SELECTION.json`
- 台账：`/cursor/stores/store-c5bcaa91-b7a1-451f-aa70-1f3949024884/v25-c-preprocess-fixtures/FIXTURE_LEDGER.json`

## 夹具身份

- 基于已准入 TRAIN400 manifest SHA `db2c85bd16fe777cd39bdff8d1fdf96f5e9ade4ce263bdc35c510becff8435e7`。
- 读图前冻结：全部 21 个 explicit double；该 `rows[]` 顺序下前 21 个 single。SU 在该 manifest 中为 0（400 条全是 `source==special`），不从验证集或其他列表补样。
- 42/42 源文件 SHA 与 manifest 一致，均可解码，rectify 中间值由原 crop 路径在拼接前截取（21 个 double 的 cropped 尺寸均不等于 rectified；21 个 single 不拼接）。缺样 0，未换样。
- `source==su` 是数据来源标签，不是线上语义；未发明运行时 source 字段。
- OpenCV 5.0.0 解码已准入 JPEG 时打印了 `Corrupt JPEG data: premature end of data segment`；文件 SHA 仍匹配且 `imread` 成功。已记入台账，未因此换样。
- 交接仅有 rectified / expected96 / 编号尺寸 source·layer / 训练器小片段与可导入 reference。不含模型、权重、ONNX、PT、axmodel、视频、原始全帧、GT 文字颜色、验证集、备份或凭据。

下一 Cloud 轮：限定 CPU，用现有 `merge_double` 几何 helper 对照；不完整工程构建，不重跑 176/32。本登记不启动该调用。
