# B17 — 单次公开 ONNX float-run 兼容性诊断

完成：2026-11-17T10:12:34.772109+08:00（Asia/Shanghai）。本次公开CLI已真实执行，失败后立即总停止，未换路、未重试。

## 冻结与输入保护
- 原B17提案原文保存在 `PRIOR_B17_PROPOSAL.json`，SHA256 `4ee141c5eea8476e4fdf3156631a4ec09f32c4f6bd75d141f501cdd1f4b96520`；修订十项合同 `FROZEN_CONTRACT.json`，全部命令 `FROZEN_COMMANDS.json`。
- 主假设仅公开ONNX入口能否执行及其对B11学生ORT的部分数值差异；没有把preobserver全部语义当准入条件，没有精度修复承诺。
- 原学生SHA256 `9c840305ea6f5cb439b78ac350c05cb2f1674357bb47652e34f2f9de83600827`，64输入全部重新核实hash、FP32、1×3×96×96、finite；原图只读，无改图/导出。
- 固定list SHA256 `79058a9c85165031f1909a84a69c42346d31f1f7bddb032aae3362aa2f0b2648`，首例CAL_0000；一个公开CLI预冻结枚举64，不另做smoke再跑64。
- 复用既有rootless运行时，只读rootfs并以tmpfs遮蔽/root；未挂载/访问private_root或许可材料，未装版本/改源码/调用内部API。

## 实际结果：数值证人拒绝
- 公开 `run --help` 调用1次，exit0，实际再次宣告ONNX、--config、--list、--enable_perlayer_output。日志 `help_STDOUT.txt`。
- 模型公开CLI只调用1次，exit1；没有进入CAL_0000，没有任何case开始日志或输出raw，完成 **0/64**。
- 原样异常见 `model_STDERR.txt:1–9`：
```
run.py:95 -> InferenceSession
sim.py:232 -> gen
sim.py:215
AttributeError: 'NoneType' object has no attribute 'hardware_type'
```
- 这是本次exact原图/版本公开路径的真实构造阶段失败，与先前公共源码静态疑点相符；不扩大成所有ONNX或全部V8不可用，也不是license错误。
- 按异常栈和已公开控制流，失败在case循环/ss.run之前：样本forward完成0、开始0；没有注入内部计数器，因此不伪称测量了隐藏内核活动。
- 输出清单 `OUTPUT_RAW_MANIFEST.json` 为空；`COMPARISON.json` 的MAE/Max/RMSE/Cosine/argmax全部null（缺失，**不是0误差**）。无额外ORT调用。

## 实测资源
- 模型CLI 12.749589s；help 19.654323s；CLI合计 32.403912s。
- 模型树RSS采样峰值 630763520 B（0.587GiB）；help峰值 610533376 B。
- 最小MemAvailable 13321142272 B（12.406GiB）；CPU限定[0, 1]。没有触发资源停止，PSI仅记录。
- 模型wall120s、树RSS2GiB、MemAvailable≥1GiB门不变；采样日志/回执完整保留。生成点stage用时146.949s，≤600s。

## 保护与下一步
- 训练0、量化/build0、compile0、内部API0、额外ORT0、重试0；不需要NPU/板/PC，不访问生产仓库，不修改权重/参数/架构/分割/真值。
- 原CAL64对原float仍Max=.04685291647911072、MAE=.005205561543925796、argmax63/64（继承R84/B11，未重测）；原.03/.005/64永久门不变，仍NO-GO。
- CAL64=SOURCE64参与464校准、搜索、调试及数值门事实不掩盖；没有用于训练/scale选择。此轮没有可接受的部分数值证人，更不代表模型/NPU验收。
- `OFFICIAL_SUPPORT_GAP.json`更新为真实公开CLI复现失败，以及独立preobserver接口/图阶段/输出/调用数说明需求；只报父，未外发。B此支待官方支持说明，不能patch/内部替代或重试同失败。
- 不新增训练/build提案，不重复无信息报告或哈希循环；A/C独立继续。本轮不需用户补图/标注。
