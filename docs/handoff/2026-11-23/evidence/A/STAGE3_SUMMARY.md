# A 第三阶段：生产 rec lineage 预检与纠正后的冻结提案

时间：2026-11-17T09:47:40+08:00，时区标签：Asia/Shanghai (UTC+8)
目录：/workspace/cn_val_su/v25/delivery_20261117/A/stage3_lineage_preflight_20261117

## 原提案保留

`/workspace/cn_val_su/v25/delivery_20261117/A/stage2_archive_close_20261117/EXPERIMENT_PROPOSAL_V24_VAL600_PREFIX_TTA.json` 未覆盖。SHA256 `b8642174da52ce6f5810f8492e9908b5ac08459633f1d33b10c7f5f4973943f6`，62 行，6030 字节，状态仍为 DRAFTED_NOT_EXECUTED。

## 身份门纠正（已冻结，不覆盖原文）

原提案第10项把“生产权重 SHA 必须不同于 f346/45b005/0565e53a”写成身份门，逻辑不正确。生产权重可以和某个既有候选相同；不得因为相同而判错，也不得因为不同就称生产。身份必须由可信生产 manifest、R34 COM3 现场 SHA、v2.4 板端包哈希、以及 R44 八项/七模型备份的精确追溯决定。本纠正只改身份判定规则，不改变单变量冻结软件基线设计，也不覆盖原提案文件。

纠正后的 10 项提案：`EXPERIMENT_PROPOSAL_V24_VAL600_PREFIX_TTA_CORRECTED.json`。冻结合同：`FROZEN_CONTRACT.json`。纠正说明：`IDENTITY_GATE_CORRECTION.json`。

## 预检结论（板外定向 R34–R44/R45，未重扫 290 报告）

生产默认 rec **.axmodel 身份已绑定且有效**：

- 路径：`/opt/demo_v24/ax630c_plate_face_demo_board/models/plate_rec_npu1.axmodel`（`run.sh` 默认 NPU=npu1；现场别名 `/opt/cn_val/m/orig_rec.axmodel`）
- SHA256：`728d8f2065632dbd383c347d80f4fba12a38d270da9289223cc709285ba66576`（3596122 字节）
- 与 R34 COM3 现场 SHA、v2.4 板端包成员及包内 SHA256SUMS、R39 备份覆盖、R45 production_v2.4 行、以及板外已有 `improve/stage/orig_rec.axmodel` 的本阶段复哈希一致
- npu2 为备选，SHA `298ccc62bae56ca22d743bb74cd8409e11fce876d460521780c4fd6ecf691f59`，不是默认

R44 八项/七模型备份中的 F7 `rec_real_minmax.axmodel` SHA `02a2f4f181934c08e1db0ab720dcb5bc0e63b48fc5f59f5ef71e774e52f3f030`（本阶段复哈希一致）**不是**生产默认 rec。不得仅因已备份而称生产。生产默认 rec 不在这七个模型里，而在 v2.4 板端包备份中。

**官方浮点对应链未找到。** v2.4 包内该 rec 只有 .axmodel；附近 `plate_rec_rpv3_48x160.onnx`（`0d1d5007fd89da888f929b80bc681d7cd996c4a15652d77d5b7d0a321c3774e6`）及已接受 f346/45b005 均无“是 728d8f20 官方 export 源”的证明。不得把裸 .axmodel 伪装成 ONNX 软件对照，本阶段不重新导出、不训练、不外网拉模型。

无“同生产权重 + 冻结 val600 + 锁定 Prefix+TTA”的现成结果可复用。不得把已接受 554/600 在缺少官方对应时当作该格。

## 推理

**未执行。** 预检不能满足“可信对应生产实际 rec 权重、现成支持 Torch 或 ONNX、接口与冻结契约完全匹配”。600×6 软件推理一次也不跑。该格保持 **UNKNOWN**。

这不是 NPU / 全帧 / 视频通过，也不是发布接受。

## 精确缺少的产物（不是“需要用户所有数据”）

1. **官方 export/compile 对应 manifest**：写明哪一份已存在的 ONNX 和/或 PT 是生产 `plate_rec_npu1.axmodel` SHA `728d8f2065632dbd383c347d80f4fba12a38d270da9289223cc709285ba66576` 的官方导出源。来源：既有板外 Pulsar/AXERA 编译包、官方导出日志、或同一官方导出留下的已证明 sidecar。不是本阶段新建重导出。
2. **那份已导出的浮点文件本身**（ONNX 或 PT 有其一且接口匹配即可），须与冻结 48×160 输入及锁定 Prefix+TTA 契约完全匹配。

不需要为此格提供新标注、新抓拍或用户全部数据。也不需要用户再批准已接受的单变量技术选择。

## 接受/拒绝（仅基线身份有效性）

- 生产默认 rec axmodel 身份：`ACCEPTED_VALID`（`728d8f2065632dbd383c347d80f4fba12a38d270da9289223cc709285ba66576`）
- 将 R44 F7 rec_real_minmax 当作生产身份：拒绝
- 将未证明对应链的附近 ONNX/PT 当作 728d8f20 官方浮点：不接受
- 本阶段软件填格：未执行，格仍 UNKNOWN
- 发布接受：不在范围

## 资源

预检自 2026-11-17T09:45:18+08:00 至 2026-11-17T09:47:40+08:00，远低于 15 分钟与 2CPU/2GiB 帽。未启动推理，无触帽、无重试。
