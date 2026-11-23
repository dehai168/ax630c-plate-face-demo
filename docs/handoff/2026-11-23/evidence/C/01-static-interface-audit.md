# C 线静态接口与视频调用链审计

- 时间：审计只读，未编译、未推理、未训练、未改文件。
- 分支：`cursor/v25-c-integration-audit-50d6`
- 提交：`3114764f0ee64ea576cc008a56748a0520194e81`（父提交 `08eb9d6baa8e3c611dc8f3230df80fa1dcdecd07`）
- 六文件 SHA 复核仍为 R40+R46 预期值，工作区干净。
- 已证实 bug：无。默认 `board/run.sh` 不传 `--color5`，旧三类选择头、普通/新能源/已有特殊字与白黑规则仍走 v2.4 路径。下面的缺口是未覆盖接线，不是已观察到的错误输出。不写修复提案。

## 身份（整仓不是 v2.4）

| 对象 | 提交 / SHA | 与本分支的关系 |
|---|---|---|
| v2.4 剥到的提交 | `f848b7d5fe1470a91c684f4ba5b115451b675ea3` | 祖先。六份 C++ 源在 `3114764` 之前与它相同 |
| main 祖先微调 | `1711ff5178672109f45559aaf9155bb7b8bf6437` | 祖先。加入 `models/onnx/plate_rec_finetune.onnx` SHA `45b005b56e92df83bbba2199e97935c7897ff23996905d6e6eed0b3ea3af4ae1`、`plate_color_finetune.onnx` SHA `9d41c7dc9ecffb9e93c2937b1d90e6094d3d2fe4586e716ebc383e62af52f1cf`、`scripts/infer_plate.py` |
| 本提交 | `3114764` | 只增加/修改六份源文件。不改上述 ONNX，不新增 axmodel |
| 板端默认颜色 | `models/plate_cls_${NPU}.axmodel` | `board/run.sh:35`。这是 v2.4 三类头，不是 `1711ff5` 的五类 ONNX |
| 版本字符串 | `PFD_VERSION "v2.4"` | `src/version.h:2` |

`docs/finetune_20261107/special_train_report.md:13` 写明旧顺序蓝/绿/黄，微调候选顺序蓝/黄/绿/白/黑。R84 的 CAL64 失败是交接事实，该候选不能启用、不能发布。本审计没有打开 ONNX/axmodel。

## 契约矩阵

覆盖列里的 R40/R41/R46 只表示交接已声明的离线范围。本审计没有重跑 176 组、32 张、43 fixture、54 段。

| 契约 | 预期 | 代码实际 | 位置 | 离线已覆盖 | 真实 NPU 未覆盖 |
|---|---|---|---|---|---|
| 检测输入 | BGR 帧 letterbox 后按 RGB、/255 送检测 | `det_norm.to_rgb = true`，均值 0 方差 255 | `src/main.cpp:446`；ORT `src/infer/engine_ort.cpp:39-46`；AX 只做 BGR→RGB，mean/std 在 axmodel 内 `src/infer/engine_ax.cpp:78-83` | 无。R41 不是检测 | 全帧与 ROI 两次 `plate_det->detect` |
| 识别输入 | 拉正图等比到模型高、宽不超过模型宽，右填 128；BGR，(x-127.5)/127.5 | `run_rec` 用 `input_h/w`，不写死 48×160 | `src/plate_rec.cpp:103-104,152-159` | R41 未走拉正。R46 的 JPEG 不是这条视频入口 | AX 不在 C++ 里做 (x-127.5)/127.5 |
| 旧三类颜色输入 | 拉正整牌 resize 到模型高宽；BGR /255；下标 0/1/2 = 蓝/绿/黄 | 仍是 map3，且 `size()>=3` 就读前 3 个 | `src/plate_rec.cpp:193-208` | v2.4 历史路径。R41 的 32 张是五类预处理对照，不是这条 map3 | `cls_->run` |
| 五类颜色输入 | 仅在显式 `--color5` 且旧 `--color` 同时存在；96×96 BGR /255；下标 0..4 = 蓝/黄/绿/白/黑 | `prepare_color5_input` 只 resize。概率和须在 1±1e-3，否则 status 非 ok、不投票 | `src/plate_rec.cpp:15-38,112-115,302-303`；顺序与 `special_train_report.md:13` 一致 | R40 合成引擎、真实模型调用数 0。R41 从已经是 96×96 的 BGR 进入 ORT，不含拉正 | `cls5_->run` 整段不存在于默认脚本 |
| 五类默认 | 不配置则不加载，回退 v2.4 三类+HSV+规则 | `color5` 默认空字符串。`run.sh` 只传 `--color` | `src/main.cpp:35,109,456`；`board/run.sh:31-36`；`board/demo.cfg` 无 color5 | R40 含“默认关”。本审计只看到源码默认值 | 默认脚本不会进入五类 NPU |
| 错误 schema | 五类模型尺寸不是 96 或没有旧三类时，整个 load 失败并返回 3 | 已实现。旧 `--color` 槽对“长度≥3 的五类张量”不拒绝，仍按蓝/绿/黄解释前三个 | 拒绝：`src/plate_rec.cpp:101,115`，`src/main.cpp:456`。不拒绝：`src/plate_rec.cpp:204-208` | R40 的负控若覆盖了合成 schema，未在本机重放。旧槽误接五类文件没有对应测试编号 | 两种接法都没有 NPU 结果 |
| 层数选择 | 双层是否改单层仍由旧三类/HSV，不用五类头 | `classify_color` 写入 `pre_color`；五类在规则之后单独跑 | `src/plate_rec.cpp:230-275,278-303` | R40 的 19×8 配置矩阵声称覆盖层数配置；未重跑 | 判层用的是旧 `cls_` |
| 文字权重 | 五类开启后，票权仍乘旧 `rr.color_conf` | 注释写明不把五类绑进假设或文字权重 | `src/main.cpp:325,762` | R40 声称不让五类改变选择权重。未重跑 | 权重不经过 `cls5_` |
| 白黑与特殊字 | 警白、学挂黄、使领港澳黑；WJ 白、粤Z 黑保持原生产范围；不新增军牌、应急 | `crop_policy::resolve` 后再次强制 WJ/粤Z。inc 里没有军、应急。WJ 语法返回 -1，走旧校验 | `src/color5_crop_policy.inc:2-10`；`src/plate_rec.cpp:40-47,283-285`；`src/approved_special_format.inc:4-5` | 旧规则在 v2.4。R46 的 43 fixture 针对派生格式，本审计未重跑，不能把每一条都标成已逐条证实 | 规则在 CPU。NPU 只提供 raw5 |
| 普通/新能源格式 | 不含特殊字时 `approved_special_syntax` 返回 -1，旧 7/8 位校验不动 | 返回值 `<0` 才落入原函数 | `src/filters.cpp:45-48`；`src/plate_rec.cpp:86-89`；`src/approved_special_format.inc:8` | R46。`tools/test_rules.cpp` 仍是 v2.3 黄绿用例，源码未改，也未编译 | 无 |
| 使馆六位数字+使 | R46 已批准的语法返回 1；旧“省+字母”不再是这条的必经路径 | 仅 7 字且末字为使、前六位为数字 | `src/approved_special_format.inc:14` | 归 R46，未重跑 | 无 |
| 跨帧颜色 | 五类开：raw5 同时进入 `color_votes`（乘 conf）和 `cls_votes`（不乘 conf）。五类关：仍用规则后的 `rr.color` 与 `rr.cls_color` | 帧上再调用 `resolve_plate_color5` | `src/main.cpp:326-333,764-768` | R40 是单次 recognize 配置，不是 `min_vote`/`valid_ratio` 跨帧。R46 的 54 段 main/324 帧不是这条 NPU 视频 | 投票数组不在 NPU |
| 画面 / snap / 合并 JSON | 三处都读累计后的 `final_col`，不读帧级 `display_color5` | 该字段只写不读 | 画面 `src/main.cpp:832-838`；snap `381-382`；合并 `573,912-924`；写入 `src/plate_rec.cpp:303` | R46 的 222 张合成 JPEG 不是 `VideoWriter` | 编码在 CPU |
| Python 裁图契约 | 只对照仓库里存在的 `scripts/infer_plate.py` | 单层：96×96、BGR、/255，顺序蓝黄绿白黑，与五类头一致。双层：Python 先横向拼接再上色；C++ 五类头看的是拼接前的拉正图 | Python `scripts/infer_plate.py:25-32,41-49`；C++ `src/plate_rec.cpp:28-30,303` | R41 覆盖已裁 96×96 的 ORT 前处理，不覆盖双层拼接差异 | Python 脚本不加载 axmodel |
| 资源生命周期 | 引擎在 `load` 创建，`unique_ptr` 随 `main` 块结束销毁；AX 另有 init/deinit | 五类 load 失败则 `cls5_.reset()` 且整个识别器 load 失败，视频不会带半套模型开跑 | `src/plate_rec.cpp:99-117`；`src/main.cpp:440,456,934`；AX 释放 `src/infer/engine_ax.cpp:91-103` | R41 是库级 ORT 调用，不是整段视频句柄 | `AX_ENGINE_RunSync` 未跑 |
| 工具入口 | `image_eval` 仍是两参数 `load`，打印的是旧 `r.color` | 现有工具不能打开 `--color5` | `tools/image_eval.cpp:39,86-95` | 该工具不在 R40/R41/R46 的视频替身里 | 无 |

`display_color5` 无读者不是默认路径算错。五类开启时，画面颜色来自累计票再走 `resolve_plate_color5`。8 位且最终为绿时，`src/main.cpp:769` 把 `color_src` 从 `rule5`/`color5` 改写成 `rule`，颜色值不变。这是标签折叠，没有第二套颜色结果。

## 视频 / NPU 调用图

```
main 440 backend_global_init
  ORT 构建：空操作
  AX 构建：AX_SYS_Init + AX_ENGINE_Init     ← NPU 运行时，R40 合成引擎未覆盖
main 489 cv::VideoCapture
每帧:
  615 face_det->detect                      ← NPU/ORT 一次
  631 plate_det->detect 全帧                ← NPU/ORT 一次
  658 plate_det->detect ROI                 ← 每个 ROI 再一次；默认 demo.cfg roi_max=1
  ByteTrack（CPU）
  749 recognize(frame, kps, layer)
      rectify 透视（CPU）
      旧三类 classify_color：resize + cls_->run
      双层拼接只进入 run_rec，不进入颜色
      dual_hyp 可能再 run_rec 一次
      规则写 rr.color（旧头）
      仅 cls5_ 已加载：prepare 96×96 + cls5_->run + decode 0..4
  762 文字票仍乘旧 color_conf
  764 update_plate_color_votes
  768 final_col
  832 画面标签 final_col
  873 VideoWriter
轨迹结束 573 把 final_col 放进 PlateResult
912 合并后再 resolve 一次
934 backend_global_deinit
```

断点：默认脚本停在“不创建 `cls5_`”。五类 NPU 调用只有命令行或配置显式给出 `color5` 且 load 成功才存在。`run.sh` 与 `demo.cfg` 都没有这个键。

R40 替换的是 InferEngine 替身。R41 从 96×96 缓冲区进入 ORT。R46 的 54 段不是 NPU 视频。三段都不覆盖上图里的 AX `RunSync`。

## 最小后续离线验证提案（本阶段不执行）

不要重跑 176/32/43/54。不要加载 `plate_color_finetune.onnx` 或任何 axmodel。R84 仍失败。

在将来单独允许编译之后，只编译已有的规则/解码单元，向 `decode_color5_probabilities` 喂合成 float，不建 Session：

1. 长度 5、和为 1 的概率，下标 0..4 依次得到蓝黄绿白黑。
2. 长度 3、长度 6、和不为 1、含 NaN，status 都不是 ok，且不产生颜色。
3. `color5` 空字符串时 `classify_color` 的 map3 仍是下标 1→绿。用静态阅读已能看到；编译测试只锁这条不被以后的改动碰到。
4. 早停：任何一步需要打开 ONNX、axmodel 或视频文件就停止。成功只说明 CPU 解码表。失败不解释成 NPU 或生产失败。
5. 预算沿用 2 CPU / 2 GiB。性能门槛不设。颜色头多一次推理的毫秒数在本仓没有来源，标 UNKNOWN。v2.4 CHANGELOG 里的检测 26.5→35.1 ms（+8.6 ms）只属于 U16 检测模型，不能挪作五类头指标。

## 回退与发布缺项

不能发布，也不要把 `3114764` 说成整仓等于 v2.4。

- 发布缺：五类 axmodel、通过的 CAL64、R31 未拒绝的识别器、`run.sh` 中的 `--color5`、版本号升级、包内 SHA256SUMS。后四项本提交都没有。CMake 未改。
- R84 候选文件在树上的身份是 `1711ff5` 的 `plate_color_finetune.onnx`（SHA 见上表），不是六文件之一，也不是 `run.sh` 的 `--color`。
- 回退源码：把分支重置到 `08eb9d6` 即去掉六文件差异，微调 ONNX 仍在。
- 回到 v2.4 标签树：还要去掉 `1711ff5` 与合并提交 `08eb9d6`。标签 `v2.4` 本身未被移动。
- `main` 仍是 `08eb9d6`。未合并、未部署、未开 PR。

## 下一就绪任务

无代码任务。协调方若要继续，只授权上一节的合成 float 解码检查，并且明确允许编译。在那之前不改 `3114764`。
