# B14：SOURCE64输入一致性与颜色双层输入契约

完成：2026-11-17T09:59:27.235361+08:00（Asia/Shanghai）。先冻结十项单假设合同及命令。**模型调用0、训练0、编译/量化构建0、新模型0**；没有板/PC/仓库/private_root/凭据访问。原CAL64 Max≤.03、MAE≤.005、argmax64/64不变，R84拒绝不变，不换golden或分割。

## 1. 全64输入实测

- 固定464 tar与membermanifest SHA一致；仅解码SOURCE64的64份已有PNG，不打开其余400照片。PNG与R84实际解包dataset/data/SOURCE64各文件SHA一致；公开loader按路径排序取前464时全64均包含。
- 64份PNG全部96×96、8bit RGB、各chunk CRC通过。OpenCV imread（BGR）与imdecode一致，解码像素与固定raw逆重建U8逐元素完全相同。
- 按公开loader复现：`cv2.resize96×96`（默认linear，尺寸不变故像素差0）→BGR不换色→`(U8-[0,0,0])/[255,255,255]`（NumPyfloat64）→astypefloat32→batch/permuteNCHW→C序raw。
- **64/64：字节差0、元素差0、Max/MAE=0**。每例PNG头、dtype/range/每通道SHA、每阶段shape/stride/contiguity/逻辑rawSHA与差异全保存于ALL64_INPUT_STAGE_COMPARISON.json；decoded_BGR_U8和reconstructed_NCHW_F32保存复现输出。
- HWC连续；permute形成NCHW非连续view；C序落盘连续，逻辑值不变。没有额外整数round；只有U8解码/同尺寸resize与float32 cast。公开torchhelper是tensor构造后permute，不是本轮实际执行的官方内部API。
- 脚本耗时0.448613s；到脚本结束整阶段146.550s；RSS峰值103424000B，CPU亲和[0, 1]、OpenCV2线程，均低于2GiB/600s。

## 2. 证明来源与未知边界

公开V8 wheel固定SHA `e12b139f6b4fc6d573c3f4cd96c87db0a93647b3114b26b500a20c46c6038ab0`，只解出源码阅读，不导入内部API：

- `official_public_dataloader.py.txt`，AllinDataloader._get_file_list 111–122：递归图片排序/取calibration_size；_load_feature 158–170：cv2.imread、按tensor_format查换色；_convert_image_tensor 124–156：resize、可选cvtColor、mean/std、float32、unsqueeze/permute。16–20行BGR=-1，因此不交换通道。
- `official_public_tensor_conversion.py.txt` 345–378：NumPy转torch.tensor；不冒充当前官方runtime实测。实际R84 control与quant_axmodel.json留存的实际input_configs均为mean0/std255、BGR、calibration_size464，已双向绑定；见ACTUAL_INPUT_CONFIG_AND_RECIPE_GUARDS.json。

**已证**是“归档PNG/固定raw + 公开V8实现逐步复现”相等；**未证**历史执行中原loader全部64实际tensor dump的逐bit重放，因为它们未保存。当前解码库OpenCV5.0.0，未声称与官方包内解码库逐字节同版。相等不能推出其余Conv/Resize/累加/重定标内核位精确，也不能把relu7范围gap归咎PNG加载。

原canonical浮点ONNX已有BN融合：8Conv、0BatchNormalization；`export_color_v2.py`12–15行BN-fold公式用现存best_color_state.npz重算，**18个权重/偏置initializer与ecba原float完全一致**。R84 frontend保留同28个节点的算子类型计数和全部24initializer值，但protobuf并不完全一致，后续B15专查如下。

## 3. 给C的最小、模型身份受限证据

**对于canonical原float `ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360`，special双层颜色训练代码吃的是已横拼图，而非仅原始rectified双层图。**

- `/workspace/cn_val_su/finetune_original_v2/optimize_plate_v2.py`7–8行导入common.Data，35–36行颜色训练使用Data(train,'color',True)。
- 同目录`plate_optim_common.py::crop`9–18行：先9–15行读取/四点矫正，16–17行检测`layer=='double'`，取上部前5/12、下部从1/3开始，把上部resize至下部尺寸后**横向concatenate**。11行`source=='su'`直接return，不能把special规则硬推到SU。
- `Data.__init__`20–30行：24行在rec/color分支前统一crop；29行颜色resize96×96。32–36行转float32/255、CHWcopy。故color并不绕过双层拼接。
- `export_color_v2.py`7–16行仅导出内部RGB/48×192归一化和BNfold，不会撤销外部双层拼接；本轮18参数精确匹配将该导出路径与canonical原float静态绑定。
- 只投影既有R55已准入TRAIN400 manifest的source_key/source/layer/group_id：**single379、double21**，存在显式double标记。例special:18375、special:16528、special:19413；准确清单/manifesthash见CROSSLINE_DOUBLE_COLOR_EVIDENCE.json。不读新图片、真值/文字/颜色，不扩大split扫描；此数量不是整个训练集分布或历史逐图实际执行证明。
- R82归档`train_qat_distill.py::crop`92–111及326–327行同样横拼后resize，学生SHA `9c840305...`。

**canonical ecba、学生9c840均未与repo color9d41c7建立模型身份对应。** C云端infer_plate.py行为及R40 C++输入差异是父传上下文，不是本轮repo审计结论；不得直接改C++，必须先绑定repo9d41实际训练/导出契约。

## 4. 按授权只追加一个最有价值机制核对：B15

B15另行先冻十项单假设合同：frontend节点protobuf不等是否代表影响数值的优化。零模型调用。

实际发现opset **13→18**；13个节点的原始attr差异仅8Conv显式group1、4MaxPool显式ceil_mode0、Gemm显式alpha/beta1/transA0，均ONNX默认值。strict跨版本默认dict比较27/28相同；余下Resize原始attr/边/常量不变，v18新增默认antialias0、keep_aspect_ratio_policy=stretch，对当前linear/half_pixel/固定sizes是预期的旧行为。见B15/RESULT.json及SCHEMA_ADJUDICATION.json。

**未找到改变计算配置的证据；不是内核数值一致性实测。** 不能因protobuf差异就重建，也不能因默认值可解释就断言官方校准float executor与ONNXCPU/Reference逐bit一致。

## 5. 决策与下一精确缺口

输入重建路径与frontend静态配置这两条可执行假设已排查，不再无限重哈希、盲重训或build。B11已经证明函数漂移/量化残差及抵消，本轮没有产生可改善候选的独立修复证据。

剩下有价值的**数值证人**是同一固定输入下“官方calibration float executor的observer之前input/关键层输出与observer min/max”对现存B11学生float；当前只保存了一个自然图debug样本，不含全CAL64 preobserver证人。必须先确认公开支持的捕获/precision-analysis入口，若无则明确需要官方文档/支持说明，而不是私有API、源码注入、license/root遍历或新量化重跑。当前范围不启动新调用；后续单主假设诊断需独立完整十项提案，修复训练/编译更不得直接启动。

**当前无需用户补照片/标注/设备操作；不是项目停止。** 父可继续C模型身份绑定和A/C独立交付。B与根队列只更新B引用，不覆盖A/C结果。
