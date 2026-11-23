# B11：固定CAL64学生float函数漂移与量化残差分离

完成：2026-11-17T09:50:11.041150+08:00，Asia/Shanghai。诊断而非修复/选模。执行前冻结FROZEN_CONTRACT.json及FROZEN_COMMAND.json，核验原学生SHA `9c840305ea6f5cb439b78ac350c05cb2f1674357bb47652e34f2f9de83600827`、64输入、3328个既有原float/R84 raw与schema。观测图SHA `1dce2d45ceaf074f00575811d88227eb35a578e24a5b1e65f1a608d011406c3b`；只增加28个原有节点输出，恢复原输出后protobuf与原学生完全相同，拓扑/initializer/参数不变，绝非新候选。

## 实际执行与保护

- 单session、batch1、CPUExecutionProvider，**started64/completed64，额外forward0**；每样本一次run同时取得28层。输出1792份raw/逐份SHA/有限检查/输入SHA/完整calllineage及stdout/stderr均保存。
- 模型forward阶段（保守包括session建立、检查、写raw）1.444555s；完整worker 1.587161s；树RSS采样峰值93523968B，worker自报峰值93442048B，CPU亲和[0, 1]，ORT intra2/inter1，未触及2GiB/120s。分析峰值77180928B，后续分析无模型调用。
- ORT_DISABLE_ALL与R69原float观测策略一致；原golden历史intra4、本次intra2，未为线程对照追加任何forward。前处理rgb/resize/center学生与原float输出逐元素相同。
- 原floatgolden、学生权重/架构、原图、真值、分割、原CAL64门槛全部不改；训练0、编译0、量化构建0；无板端/PC/生产仓库/private_root/凭据访问。

## 三组全64终端指标

| 比较 | MAE | Max | RMSE | Cosine | argmax |
|---|---:|---:|---:|---:|---:|
|Fstudent_minus_Fsource|0.00481426770723|0.0413184762001|0.00801424337554|0.999678262279|62/64|
|Qstudent_minus_Fstudent|0.00691813753581|0.0766730904579|0.0116864658393|0.999318018944|63/64|
|Qstudent_minus_Fsource|0.00520556154393|0.0468529164791|0.00866884993059|0.999625298746|63/64|

发布黄金仍是原始float。Qstudent-Fsource逐case Max/MAE与R84旧记录**差异0**，仍拒绝（Max≤.03、MAE≤.005、argmax64/64永久保持）。其他两组仅诊断，不能偷换黄金；其自身也都未通过全部同数值界限。五类真实准确率/物理NPU/视频验收没有新增成绩。

1216个case-layer、全部元素满足向量恒等式 `Q-F=(S-F)+(Q-S)`，Max重构误差0；终端两个误差向量cosine=-0.670804299476，存在显著抵消。不能用0.07667减0.04132等范数相减计算“贡献比例”。

## 已闭合的根因证据

- **已验证**：不是只有量化误差。学生float对原golden自身已有Max0.0413184762、argmax62/64；Qstudent对学生float另有Max0.0766730905、MAE0.00691813754、argmax63/64。R84较好的总残差部分来自两分量抵消，不是任一分量已解决。
- **已验证，非合同错配**：R82目标是fake-quant学生→冻结teacher，不是要求裸学生float严格恒等原float；漂移可能是预补偿本身。S8/S32核心公式此前FP32逐码匹配结论不变，不能借本诊断重训同一实验。
- **已验证的第一边界**：学生/原float的rgb/resize/center差0；relu0的Q-S Max0.1414157678、RMSE0.0134073190，最差通道11；同层S-F Max0.0190849304。logits的Q-S Max0.2284353115/RMSE0.0649468187。完整全层/全通道三组统计见LAYER_THREE_WAY_METRICS与CHANNEL_THREE_WAY_METRICS，不按失败ID选层/调参。
- **范围/饱和证据**：用学生自己的激活而不是原float作比：relu0/relu3/relu14均无超官方范围4LSB；relu0虽有465上端码，不能认作裁剪证明。relu7有30个学生值超过官方上界4LSB，30个均对应官方上端码，最大超界0.0134882497。这是局部饱和一致性证据，不是实际量化卷积pre-requant值或终端失败因果证明，不能因此直接改range/observer。

## 已自动推进的B12：训练/官方网格转移（只读新raw，零额外forward）

冻结训练relu0网格上界13.5413335；本次学生CAL64最大34.3927002，116095/18874368个元素超过训练网格4LSB，而超过官方relu0范围4LSB为0。官方/训练scale比例2.542999270。relu3比例1.430006451，6240个元素超过训练网格4LSB；relu14比例1.003274304。见TRAINING_OFFICIAL_RANGE_TRANSFER.json。

**推论而非已证因果**：训练3348冻结网格与官方464重观察网格明显不同，可能破坏预补偿转移，正是R81已披露风险；本次没有运行冻结网格fakequant反事实，不能宣称这是唯一根因。CAL64就是464里的SOURCE64，早已参与校准/历史搜索，仍不是独立holdout；不得拿本结果回灌CAL64/失败例训练或挑参数。

**仍未知**：权重/激活/偏置/累加/重定标各自因果量、实际累加位宽/中间截断、真实独立图像和物理NPU效果。编译PASS不等于任何上述精度PASS。

## 下一最小可执行项

B13：仅查既有量化元数据和已归档官方公开源码，解释“CAL64已在464校准中，但relu7有30个学生float值仍高于最终官方range”的精度阶段/scale-refine来源。单问题、零模型调用/训练/编译，不修改range或重开observer；找不到直接来源就保留未知。此项由父任务持续推进，A/C不等待本问题。

没有提出或启动修复训练/编译；任何后续实验须完整10项提案、单主假设、独立冻结并记录保护门。single-attempt只约束已执行实验，不能推成未来研究永久禁止。当前不需要用户补数据或操作设备。

### 持续推进：B13也已只读完成，误差来源进一步收窄

R84已有PPQ quant_axmodel.json的relu0/relu7/relu14 scale与最终ONNX序列化scale逐值相等，relu7的18.7156491709高界在PPQ阶段已存在，不能归咎最终导出scale舍入。已读官方公开归档AxRescaleWeightParameterQuantizePass，它针对小权重/偏置溢出，不提供“关闭weightrefine可修复relu7激活range”的证据。统计阶段或ImageLoader/float路径差异仍未知，不因相关性重开开关。见RANGE_METADATA_PROVENANCE.json。下一B14：只读重建SOURCE64校准PNG→BGR/255与CAL64固定raw的字节一致性，并核对可得官方loader来源；不运行模型。
