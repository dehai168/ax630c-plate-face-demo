# 勘误、双层颜色输入证据链、缺文件与不可执行提案

- 不改 `01-static-interface-audit.md`。其 SHA256 仍是 `47de55b082f0654828e427f850e703d4e85953725b8c2d9c59ca4bde27275aba`，`wc -l` 为 103。父方按 104 行阅读；被纠正的句子在该文件第 96 行。
- 分支 `cursor/v25-c-integration-audit-50d6`，提交 `3114764f0ee64ea576cc008a56748a0520194e81`。六文件 SHA 未变。
- 未编译、未推理、未改权重、未上板。未重跑 176/32。
- 双层颜色输入口径：未闭合。R84 与仓库 `9d41c7…` 的关系：UNKNOWN。

## 1. 勘误（只加本文件，不改旧文）

旧报告第 96 行写：“R84 候选文件在树上的身份是 `1711ff5` 的 `plate_color_finetune.onnx`”。这是错的。

| 身份 | SHA256 | 本仓库能否对应 |
|---|---|---|
| `1711ff5` 的 `models/onnx/plate_color_finetune.onnx` | `9d41c7dc9ecffb9e93c2937b1d90e6094d3d2fe4586e716ebc383e62af52f1cf` | 能。只表示这一份入库 ONNX。CHANGELOG、README、`special_train_report.md:86` 都是这个值 |
| R84：R82 导出的学生 ONNX | `9c840305ea6f5cb439b78ac350c05cb2f1674357bb47652e34f2f9de83600827` | 本轮未在 src/scripts/docs 中见到。与 `9d41c7…` 是否相同：UNKNOWN |
| R84 compiled | `f16afae621ba3fa54cee3ab9ac062b3e26925e6497d469fc342867d8c7c4f5a4` | 同上。与学生 ONNX、与 `9d41c7…` 是否相同：UNKNOWN |
| 源端当前 canonical 原 float | `ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360` | 同上。与 `9d41c7…` 是否相同：UNKNOWN |

`9d41c7…` 仍然不能启用为发布颜色模型：它没有 axmodel，README 写明未量化。这和“它不是 R84”是两件事。

旧报告第 40 行附近把双层“Python 先拼接、C++ 用拉正原图”记成契约差异，并用默认 `run.sh` 不传 `--color5` 把后续收成“无代码任务”。默认脚本不开五类，只说明板端默认不会走到这条输入。它不能把训练输入语义标成已闭合。

## 2. 双层颜色：仓库里实际写了什么

五类训练源码不在本仓库。全库没有 `mixed_split.json`，没有带 AdamW 或颜色张量构造的训练脚本。`docs/finetune_20261107/` 只有报告和两份指标 JSON。指标 JSON 的 `color` 只有准确率，没有 layer，也没有“拼接前/后”字段。

并存且互相不推出对方的材料：

| 材料 | 对颜色图的字面含义 | 不能推出的结论 |
|---|---|---|
| `docs/finetune_20261107/special_train_report.md:29` | 同一段写了 color BGR/255、96×96、多边形矫正，以及“双层按原上 5/12、下 2/3 拼接” | 没有写 96×96 的像素是拼接前还是拼接后 |
| `scripts/infer_plate.py:41-43` | `--layer double` 时先拼接，再把同一张图缩到 96×96 送颜色网 | 这是裁图脚本的当前行为，不是训练循环 |
| `README.md:47-53` | 脚本被写成 x86 裁图入口；“拼接后再识别”这句话的动词是识别 | 文档没有单独写颜色网吃拼接图。代码却让颜色网吃拼接后的 `im` |
| `special_train_report.md:72,78` | 交付用法是训练服务器上的 `infer_plate.py`，并写“C++ 尚未修改” | 没有把该脚本声明为唯一训练张量定义 |
| `scripts/val_tools/prep_data.py:32-34` | 识别用 `merge_double(rc)`，颜色用未拼接的 `rc` 缩到 96×96 | 路径是 `/workspace/cn_val_su/improve/data`，服务 v2.4 校准/留出，不是 2026-11-07 微调训练器 |
| `scripts/prepare_calib.py:68-69` | 颜色是拉正图直接 96×96，无双层分支 | CCPD 校准，不是五类训练集 |
| `src/plate_rec.cpp:142-149,193-201,301-303` | `merge_double` 只进入 `rec_layer`。旧三类和 `classify_color5` 都吃 `r.rectified` | 与 v2.4 颜色路径一致。没有任何注释写“为保护颜色而故意不用拼接” |
| `src/color5_crop_policy.inc:6-7` | `layer` 只参与白/黑改写：双层黄、单层蓝 | 不改颜色网的输入图 |

因此五类颜色训练的实际输入是 UNKNOWN，两个候选都不能当事实：

- 候选 A：先按上 5/12、下 2/3 拼接，再 96×96。依据只到 `infer_plate.py` 和报告第 29 行的可歧义读法。
- 候选 B：颜色用拉正后的整牌，拼接只给识别。依据是 v2.4 的 `prep_data.py` 和当前 C++。R40 沿用 B。没有证据证明这是为了保护颜色而刻意选择。

`scripts/infer_plate.py` 的角色：它是 `1711ff5` 入库的裁图 demo，SHA256 `3d00d821f43671e3ab88039a0f5547bb6be53767a36b59e3179c15e3a22c02ad`。报告把它列为使用方式。它不是视频 `main`，也还不是已证明的训练输入规范。不能按它的当前行为改 C++。

## 3. 冻结的十项单假设（只读，本轮已在 src/scripts/docs 内核过）

每一项只允许一个假设。预算已用在本轮阅读，不再打开模型。早停条件都是：要跑 ONNX、要编译或要改 `3114764`。

1. 假设：报告第 29 行已经写明颜色 96×96 来自拼接图。结果：句子同时写了矫正和拼接，没有像素顺序。不成立为证明。
2. 假设：`infer_plate.py` 就是训练时的颜色张量代码。结果：仓库没有训练器；脚本只是交付用法。不成立。
3. 假设：README 第 53 行要求颜色网与识别网看同一张拼接图。结果：原文动词是“再识别”。代码是拼接后的图也进了颜色网。文档弱于代码，仍不是训练记录。
4. 假设：`val_tools/prep_data.py` 就是五类训练集的构造方式。结果：它写的是 cn_val_su 的 v2.4 校准图，颜色不拼接。不能代表 2026-11-07 训练。
5. 假设：`prepare_calib.py` 覆盖双层颜色。结果：没有双层分支。该假设失败。
6. 假设：C++ `classify_color5(r.rectified)` 有注释说明故意避开拼接以保护颜色。结果：没有这句注释。R40 只是接在旧颜色路径之后。意图 UNKNOWN。
7. 假设：`crop_policy` 会在双层时改颜色输入图。结果：它只改颜色名字。输入图假设不成立。
8. 假设：`finetune_metrics.json` / `onnx_validation.json` 记录了双层样本的颜色输入。结果：只有分类计数。不成立。
9. 假设：仓库里的 `9d41c7…` 就是 R84 学生 ONNX 或 canonical float。结果：三个外部 SHA 都未在本范围出现。关系 UNKNOWN。此项只纠正身份，不决定双层。
10. 假设：默认 `run.sh` 不传 `--color5`，所以双层输入不必再查。结果：这只避开默认板端路径。用户要求的 Python/C++ 接口仍缺训练侧证据。该假设不能用来结案。

成功标准（尚未达到）：训练器或输入 manifest 写明颜色张量在 `merge` 之前或之后。失败标准：像现在这样只剩歧义段落和一份 demo。改善/退化：在查清之前改 C++，可能破坏 R40 沿用的拉正图，也可能继续偏离 demo。两项都还没被数据证明。

## 4. 父方可在 box 上查的文件

本机没有这些路径。不要靠再编译或再跑 176/32 来代替。

| 优先 | 路径 | 要看的内容 |
|---|---|---|
| 1 | `/workspace/cn_plate_special_20261107/finetune_original/original_mixed_finetune_20261107.tar.gz` | README 记录 SHA256 `c1d86ea45b6037a9c3fa0a396b4d66716da65d26b59e91620993ca39194e4f10`。在包内找构造颜色 batch 的训练脚本，看 `resize(96,96)` 相对拼接的先后。不要只看导出的 ONNX |
| 2 | 该 tar 内与仓库 `scripts/infer_plate.py` 同名的 `infer_plate.py` | 和仓库 SHA `3d00d821f43671e3ab88039a0f5547bb6be53767a36b59e3179c15e3a22c02ad` 比字节。相同只说明 demo 一致，仍要看第 1 项里的训练器 |
| 3 | 报告点名、仓库没有的 `mixed_split.json` | 是否含 layer=double 的图片列表。有列表也只说明样本存在，不说明像素是否拼接 |
| 4 | `/workspace/cn_plate_special_20261107/finetune_original/pipeline_color_metrics.json` | 是否记录双层子集。仓库副本不存在 |
| 5 | 同目录 `best_color_finetune.pt` | 只在需要核对权重身份时打开。本问题首先要的是数据管道，不是再推理 |

查完若训练器明确写了拼接前或拼接后，才能回到单变量改动：只改 `classify_color5` 的输入图，不动 `crop_policy` 的白/黑规则，也不动旧三类 `classify_color`。在那之前，这项修复不可执行。

## 5. 旧 `--color` 槽的 fail-closed 提案（不改代码，当前不可执行）

单变量：旧颜色模型的输出元素个数必须恰好是生产三类头的真实打包长度；不是这个长度就拒绝 load，视频不要启动。不改 map3 的蓝/绿/黄顺序，不加五类概率和检查，不改 `--color5`，不改空 `--color` 的 HSV 回退。

接口前提，缺一条就不能改：

1. 真实三类输出的元素乘积是多少。`scripts/prepare_plate_onnx.py:6` 只写“输出 3 类: 蓝/绿/黄”，输入是 `[1,3,96,96]`。`scripts/pulsar2_configs/plate_cls_npu1.json` 没有输出 shape。`[1,3]`、`[1,3,1,1]`、`[1,1,1,3]` 的乘积都可以是 3，但最后一种不能再假设前三个 float 就是蓝/绿/黄。
2. 本轮按范围没有读 `models/onnx/plate_cls_96x96.onnx`，也没有读 `plate_cls_npu1/npu2.axmodel`。所以“恰好 3”还是 UNKNOWN，不是已证明。
3. 现有 C++ 是 `outs[0].size()>=3` 后只取前三个（`src/plate_rec.cpp:204-208`）。五类长度 5 会误入。这是误配置风险，不是 `run.sh` 默认路径。
4. 拒绝条件若写成 `size()==3`，只有在生产图的元素乘积确实是 3 时才不会拆掉旧头。乘积不是 3 的合法打包会被误拒。
5. 与 load-time 检查的关系：ORT `load` 可以在第一次 `run` 之前读输出 shape；AX 要等 `GetIOInfo`。这是互补闸门，不是现在就能写的补丁。空 `cls_model` 必须仍成功，因为 HSV 是 v2.4 的合法配置（`src/plate_rec.cpp:107`）。
6. 旧头注释写“已 softmax”，但没有 schema 文件证明数值在 0 到 1 且和为 1。不能把五类的 `probability_sum` 检查搬到旧头，否则可能拒绝仍在板上的三类模型。
7. `--color5` 已经要求 96×96，并且没有旧 `--color` 时直接失败（`src/plate_rec.cpp:101,115`）。fail-closed 只补旧槽，不放宽这个前提。
8. 回归保护：`board/run.sh:35` 的 `plate_cls_${NPU}.axmodel` 必须仍能 load。做不到这一证明就不提交改动。
9. 改善只限于拒绝把五类张量塞进 `--color`。不声称颜色准确率变化。
10. 成功：有一份不推理的图输出 shape 记录，证明元素乘积和通道顺序后，再写拒绝条件。失败或早停：现在就按 `>=3` 改成 `==3`。结论：提案不可执行。

缺的 box/仓库证据：对 `models/onnx/plate_cls_96x96.onnx` 与两份 `plate_cls_*.axmodel` 做图 IO shape 读取，不跑 Session、不跑 `RunSync`。本阶段没有做。

## 6. 下一就绪任务

父方用第 4 节的 tar 确认颜色训练像素顺序。并行可做的只有三类图的 IO shape 抄录。两件都完成之前，不改 `3114764`，也不把双层缺口或 R84 身份写成已闭合。
