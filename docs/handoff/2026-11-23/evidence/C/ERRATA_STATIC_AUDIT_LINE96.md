# 勘误归档：云端静态审计报告第 96 行误把仓库 9d41c7 写成 R84

- 登记时间：2026-11-17T09:56:30+08:00（Asia/Shanghai (UTC+8)）
- 本文件只归档勘误，**不修改**原报告 `01-static-interface-audit.md`。
- 原报告保留哈希：SHA256 `47de55b082f0654828e427f850e703d4e85953725b8c2d9c59ca4bde27275aba`，`wc -l` = 103。被纠正的句子在第 96 行。
- 合同来源（只读）：`/cursor/stores/user/grok-bot/agents/5431defa-b011-4c89-8819-e5db585b41a8/assets/01M7KKBRVQJ1MWYQ8YEHQEYSMG.md`
- **禁止再引用原错误结论**（“R84 候选文件在树上的身份是 `1711ff5` 的 `plate_color_finetune.onnx`”）。

## 正确身份（字节层）

| 身份 | SHA256 | 本阶段字节结论 |
|---|---|---|
| 仓库 `1711ff5` / 训练包内 `plate_color_finetune.onnx` | `9d41c7dc9ecffb9e93c2937b1d90e6094d3d2fe4586e716ebc383e62af52f1cf` | 训练包该成员与此 SHA **字节相同** |
| R84 学生导出 ONNX | `9c840305ea6f5cb439b78ac350c05cb2f1674357bb47652e34f2f9de83600827` | 与 `9d41c7…` **字节不同**（SHA 不同即不是同字节） |
| R84 compiled | `f16afae621ba3fa54cee3ab9ac062b3e26925e6497d469fc342867d8c7c4f5a4` | 与 `9d41c7…` **字节不同** |
| canonical float | `ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360` | 与 `9d41c7…` **字节不同** |

语义/训练继承关系仍为 **UNKNOWN**，不得把“字节不同”写成“继承关系 UNKNOWN 到连是否同字节也不知道”。

`9d41c7…` 仍然不能启用为发布颜色模型：没有 axmodel，README 写明未量化。这和“它不是 R84”是两件事。

默认 `run.sh` 不传 `--color5` 只说明板端默认不会走进五类输入路径。默认路径上未证实的 bug **不等于** v2.5 全部接口已闭合。
