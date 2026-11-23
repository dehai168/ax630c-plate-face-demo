# B颜色误差溯源：首批交付

生成时间：2026-11-17T09:42:37.215697+08:00（Asia/Shanghai）。范围：仅box既有归档/模型字节/raw；训练0、编译0、新推理0，无新模型/实验，无仓库/生产v2.4/板卡/用户PC/private_root/凭据访问。

## 实测与当前判断

- 原float SHA256：`ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360`。
- R82学生（亦R84输入）SHA256：`9c840305ea6f5cb439b78ac350c05cb2f1674357bb47652e34f2f9de83600827`；R84输入副本新测相同。
- R84 compiled SHA256：`f16afae621ba3fa54cee3ab9ac062b3e26925e6497d469fc342867d8c7c4f5a4`；量化ONNX图SHA256：`4495b34b55b4f660143a6e2a51020b990a903b86f6cab672a4f7670ead7e9632`。
- 量化ONNX若含external data，完整外部参数文件已另列实测哈希于ARTIFACT_INDEX/BASELINE_COMPARISON的model_closures，不能用图文件哈希代替整个参数包。
- 原float/学生均28节点，节点protobuf及IO完全相同，18个权重/偏置initializer改变。内部BGR→RGB、half_pixel双线性48×192、减.5乘2一致。外部Reference是FP32 NCHW BGR/255，compiled配置是U8 NHWC BGR；不能混喂。64/64输入及各Reference的data dump字节跨R69/R79/R84一致。R84板上实际IO尚未知，不能借R50旧模型冒充。
- 6400份已有raw（1228272256B）逐份重哈希：丢失0、SHA不一致0。3轮各64×19层重算MAE/Max/RMSE/Cosine，并输出全通道统计；分析进程峰值RSS 87244800B，线程/亲和CPU≤2，主分析10.44s，完整raw复核3.68s；每次只载入一个case/一个tensor，地址空间硬限1900MiB，低于2GiB。

| 原float黄金→官方Reference | Max | MAE | argmax | 原门禁 |
|---|---:|---:|---:|---|
|R69 MinMax|0.06927111745|0.006630630805|63/64|FAIL|
|R79 EasyQuant|0.05534318089|0.005364822141|62/64|FAIL|
|R84 学生+MinMax|0.04685291648|0.005205561544|63/64|FAIL|

原门禁永久不改：Max≤.03、MAE≤.005、argmax64/64。R84编译/check1 PASS≠精度PASS，更不等于板端/视频/生产PASS。R84 vs R69 max失败交集仅CAL_0036，vs R79仅CAL_0059：失败迁移真实存在，不按失败身份调参。

| R84 vs 原float，层/通道 | MAE | Max | RMSE | Cosine |
|---|---:|---:|---:|---:|
|relu0 / max通道11|0.006367258513|0.1402808954|0.01306007711|0.999990355242|
|relu3 / max通道3|0.005333921105|0.1250472117|0.01262405208|0.999973421153|
|relu14 / max通道28|0.004052383837|0.08809571397|0.008811526556|0.999874974146|
|relu24 / max通道46|0.006626224017|0.2125664173|0.01434333004|0.999678774984|
|logits / max通道2|0.0367616363|0.2207369833|0.05090441461|0.999613051639|
|output / max通道0|0.005205561544|0.04685291648|0.008668849931|0.999625298746|

最大绝对误差增长是center→relu0（+0.140273266），最大RMSE增长是flat→logits（+0.038582596）；relu0最差Max/RMSE通道11，relu3通道3，relu14通道28，logits通道2。不同层维度和非线性不同，这些是描述性排名，不是各层因果贡献。

## 根因证据分级（完整见BASELINE_COMPARISON.json）

1. **已验证**：R69/R84全部9个compute层的840480个S8权重码，各自对匹配float源按FP32 `round(w/(maxabs/127.5))` 均0码差；S32 bias按FP32 `round(b/(input_scale*weight_scale))` 亦0码差。先用FP64乘/除序列化FP32 scale出现的小量“差码”，已被FP32审计解释，不是应当修模型的实现错配。R82确实实现了合同要求的BN-folded STE核心公式；不能凭此宣称完整官方卷积/累加/重定标逐bit仿真一致。
2. **已验证与限制**：官方conv0消费center码，但input_scale是center.scale×2，归一化Mul2被吸收，不能仅按同名张量解码。R84 relu0端点465，原float超过上界4LSB为0；端点占用不等于饱和。R79范围加宽仍失败，削弱“首层裁剪是唯一主因”假设。R84学生本身的量化前激活未在CAL64完整保存，其饱和尚不能独立排除。
3. **未知**：真实累加器位宽、中间shift/requant顺序及是否溢出。S32偏置不是S32累加器证明。conv17/21/24全码域松界超过S32（conv24为4329501652），这不是实测溢出；官方归档里“某些硬件int16”通用注释也不是AX620E证据。
4. **本批纠正R85推论强度**：R84输出与“原始float”比较，混合了学生函数漂移和学生量化残差；不能称已独立证明全是量化误差。找到既有R68/R79/R84 float debug的同输入TRAIN400/0000.png（special:17786），并与xrun/IO严格字节绑定：学生float-原float输出Max=0.000147998333，官方xrun-学生float Max=0.0000720631797，合并Max=0.0000859498978，向量恒等式误差0。可见分量可能抵消，绝不能相减范数分摊贡献百分比。该样本不是CAL64；全CAL64学生float raw未找到，分量大小仍未知。未覆盖/修改R85旧报告。
5. **历史已验证但不可跨范围外推**：R52官方Reference对R50真实AX旧32合成输入，Max0.000228583813/MAE0.000038288403/argmax32/32；对旧float则0.2247416973/0.0116786016/31/32。主要偏差已在编译前出现，但不是R84真实NPU验证。R53旧权重-only反事实Max0.0382709354，不能复现完整0.2247偏差或翻转，不能仅凭它宣称S8权重是唯一根因。

## 混精审计：保留失败证据，不把旧边界泛化成永久禁令

复用R55/R57/R74/R80官方归档，无新支持证据、不开实验。R55单Conv输出FP32规则被融合为U16；R57同时Conv/ReLU FP32生效，但马上以同scale重转U16再进conv3。两条具体表达已结构失败，不盲重试。

**纠正R80简写**：R57配置明确weight_data_type=S8，它并没有验证“FP32权重失败”。schema存在FP32字样≠AX620E敏感层FP32 kernel支持或有精度收益；S16枚举同理，highest_mix_precision会使layer_configs无效也不是精确层保护证据。当前不重开这些路线，只把支持未知如实列出。

## 数据职责与限制

CAL64就是464中的SOURCE64，64/64参与官方量化参数观察，R79还参与EasyQuant搜索；同时长期用于调试及数值验收，因此不是独立holdout。464=TRAIN400+SOURCE64；CAL64是源权重构造的合成压力输入，不是真实颜色GT准确率。不能换分割/真值/门槛降低难度。

R82训练3348视图/3347组，训练期激活MinMax和checkpoint选择也使用这3348；不含CAL64损失/earlystop。TRAIN400与有标签3348按source_key交集399（非400），R81已记录仅399/400可join五类标签，缺special:15898，不在本轮补标签或改分割。自然训练/量化重合并非独立验收；R55既有物理独立性覆盖缺口与JPEG警告继续保留。完整清单/哈希与职责见CAL64_DATA_ROLES.json。

## 连续推进与下一步

已经在本批自动推进：B01模型/输入绑定→B02逐层与通道→raw完整性复核→B03FP32公式审计→B04已有学生float定位→B05同输入向量分解→B07归档官方算术语义。不是写“无候选”后停止。

本批又自动完成B10：既有compilerdebug output/output_got的Max=0、MAE=0、RMSE=0；它是中间子图样本，不能强行绑定成CAL64，更不能拿check1代替验收。当前最高可执行B11是父任务审阅下一批10项诊断提案，无需向用户重问技术选择，同时A/C不依赖新模型的工作继续。

更高价值的缺口是“全64学生float漂移与量化残差分开”。队列已有单主假设、10项完整提案，**仅父审阅、未开始**；未来若批准只做现有学生float的64次CPU诊断（无训练/编译/重跑量化），严格保留原输入和门禁。本轮0次新模型调用。R81/R82一次实验冻结限制不能演绎成一切未来研究永久禁止；用户硬安全/验收限制永久保留。

**当前无需用户提供数据或操作设备。** 后续若需要真实未见失败样本、独立标注或物理NPU/视频条件，由父任务按实际缺口请求；当前诊断与A/C独立推进不受此阻塞。

## B11后续阶段（2026-11-17T09:50:11.041150+08:00 Asia/Shanghai）

已完成唯一64/64学生float诊断，单session/每例一次同时保存28层，无额外forward、训练或编译；前向1.444555s、树RSS93523968B，未触帽。详细冻结合同/命令/输出/逐层指标见B11/REPORT.md及SUMMARY.json。

S-F：Max0.0413184762/MAE0.004814267707/argmax62；Q-S：Max0.0766730905/MAE0.006918137536/argmax63；Q-F原黄金：Max0.0468529165/MAE0.005205561544/argmax63，完全重现旧拒绝。1216个case-layer恒等式误差0；两误差向量cos=-0.6708042995，存在抵消，不做贡献比例。学生relu0不超官方范围，relu7有30个>4LSB且端点对应；局部一致性不是唯一根因。训练/官方relu0scale相差2.543倍的转移风险已全64量化证实，但未做因果反事实。B12只读raw已自动完成；下一B13查既有元数据/refine阶段来源，零新调用。无需用户提供数据。

B13只读推进补充：PPQ与导出relu7scale精确相同，未发现可归因最终scale序列化的gap。根因仍未闭合，下一B14查校准PNG/固定raw字节与可得官方loader语义，不新增模型调用。

## B14/B15后续（2026-11-17T09:59:27.235361+08:00 Asia/Shanghai）
全64SOURCE64 PNG按公开V8loader语义逐步复现与固定raw字节/元素差0；这不是实际历史loadertrace或全算子bittrue。canonicalecba颜色special双层在common.crop先横拼后resize96，TRAIN400既有flag为double21/single379；repo9d41未绑定，不能据此直接改C++。B15仅一项另冻结静态核对：opset13→18/default属性解释protobuf差，未找到改计算配置证据。剩余官方calibrationfloat preobserver数值证人未归档，需要公开支持捕获入口及独立范围，不盲训练build。详B14/REPORT.md、SUMMARY.json和CROSSLINE_DOUBLE_COLOR_EVIDENCE.json。


## B16 2026-11-17T10:07:57.005700+08:00 Asia/Shanghai
官方floatdebug为量化后dequantize重跑，非preobserver现场捕获；公开ONNXrun支持与calibration执行器不同，exactV8路径待说明。本轮模型/训练/编译/量化0；B16/REPORT.md、SUMMARY.json。B17阻塞于精确官方接口/语义，修复假设不可启动，原门NO-GO不变。无需新数据；A/C独立继续。


## B17 2026-11-17T10:12:34.773201+08:00 Asia/Shanghai
已执行公开ONNX诊断：help1成功，模型CLI1失败，NoneType.hardware_type发生于session构造，完成0/64、输出0；未重试/patch/内部替代。模型CLI12.750s、树RSS630763520B、资源未触顶。B17/REPORT.md、SUMMARY.json；B支待官方支持，原CAL64拒绝不变，A/C继续，无新数据需求。
