# classify_color 函数主体 mock 结果

结论：**ACCEPT_OFFLINE_FUNCTION_MOCK_ONLY**

这不是完整运行时、NPU 或视频通过。仓库没有改动。没有新的代码 bug，因此没有另写修复提案。

## 基线

- 提交 `2241e28c110f8a10b282619459743964e8b79d4e`
- 工作树干净，六文件 SHA 未变
- 合同：`09-classify-color-mock-contract.md`
- `tools/test_rules.cpp` 不覆盖这个函数。HSV 没有单独的 helper，白/黑和色相分支都在 `classify_color` 主体里

## 抽出的原文

`src/plate_rec.cpp` 第 193–219 行，SHA256 `59d18603093a3bd840e013022a666ffaf23376b71bde20a2a739ac0279a60aaa`（1405 字节）。

函数体第 194–218 行，SHA256 `899067e52c7d9b7b49bf17b38cecf2e7e9346c8e49f139a96adc78805a1f8d48`。

颜色枚举来自 `src/plate_rec.h` 第 11 行，SHA256 `83fd3371600d8e4458294ba0d402aaf41e82471670263187a9036099fbaa6a13`。

adapter：去掉 `PlateRecognizer::`，把原文主体放进最小类的同名方法；成员仍叫 `cls_` 和 `cls_in_`。包含的是仓库里的 `infer/engine.h`。语句没有改。旧基线只把这一处 `== 3` 换成 `>= 3`，SHA256 `7e902a1105bfd73f9369620856a7070e44b50bd1c3ee075931bbfd2fb3f38e8c`，并且只在逻辑长度为 3 时调用。

mock `run` 不打开文件，不加载 ONNX 或 axmodel。输入高宽固定返回 96。

## 编译和运行

`g++ -std=c++14 -O2 -Wall -Wextra -I/usr/include/opencv4 -I src -o /tmp/ccmock/harness /tmp/ccmock/harness.cpp -lopencv_core -lopencv_imgproc`

编译 0.818 秒，运行 0.008 秒，退出码 0。`COUNT 52 FAILS 0`。

颜色编号：0 蓝，1 黄，2 绿，3 白。断言是整数和 float 的 `==`，没有容差。

## 原始日志

```
NULL blue color=0 conf=0.400000006 calls=0
LEGAL blue max0 new=0/0.949999988 old=0/0.949999988 run=1 PASS
LEGAL blue max1 new=2/0.949999988 old=2/0.949999988 run=1 PASS
LEGAL blue max2 new=1/0.949999988 old=1/0.949999988 run=1 PASS
LEGAL blue lowconf new=0/0.5 old=0/0.5 run=1 PASS
ILLEGAL blue len=0 new=0/0.400000006 null=0/0.400000006 run=1 PASS
ILLEGAL blue len=1 new=0/0.400000006 null=0/0.400000006 run=1 PASS
ILLEGAL blue len=2 new=0/0.400000006 null=0/0.400000006 run=1 PASS
ILLEGAL blue len=4 new=0/0.400000006 null=0/0.400000006 run=1 PASS
ILLEGAL blue len=5 new=0/0.400000006 null=0/0.400000006 run=1 PASS
ILLEGAL blue len=6 new=0/0.400000006 null=0/0.400000006 run=1 PASS
ILLEGAL blue len=12 new=0/0.400000006 null=0/0.400000006 run=1 PASS
EMPTY blue new=0/0.400000006 null=0/0.400000006 run=1 PASS
RUNFAIL blue new=0/0.400000006 null=0/0.400000006 run=1 PASS
NULL green color=2 conf=0.400000006 calls=0
LEGAL green max0 new=0/0.949999988 old=0/0.949999988 run=1 PASS
LEGAL green max1 new=2/0.949999988 old=2/0.949999988 run=1 PASS
LEGAL green max2 new=1/0.949999988 old=1/0.949999988 run=1 PASS
LEGAL green lowconf new=0/0.5 old=0/0.5 run=1 PASS
ILLEGAL green len=0 new=2/0.400000006 null=2/0.400000006 run=1 PASS
ILLEGAL green len=1 new=2/0.400000006 null=2/0.400000006 run=1 PASS
ILLEGAL green len=2 new=2/0.400000006 null=2/0.400000006 run=1 PASS
ILLEGAL green len=4 new=2/0.400000006 null=2/0.400000006 run=1 PASS
ILLEGAL green len=5 new=2/0.400000006 null=2/0.400000006 run=1 PASS
ILLEGAL green len=6 new=2/0.400000006 null=2/0.400000006 run=1 PASS
ILLEGAL green len=12 new=2/0.400000006 null=2/0.400000006 run=1 PASS
EMPTY green new=2/0.400000006 null=2/0.400000006 run=1 PASS
RUNFAIL green new=2/0.400000006 null=2/0.400000006 run=1 PASS
NULL yellow color=1 conf=0.400000006 calls=0
LEGAL yellow max0 new=0/0.949999988 old=0/0.949999988 run=1 PASS
LEGAL yellow max1 new=2/0.949999988 old=2/0.949999988 run=1 PASS
LEGAL yellow max2 new=1/0.949999988 old=1/0.949999988 run=1 PASS
LEGAL yellow lowconf new=0/0.5 old=0/0.5 run=1 PASS
ILLEGAL yellow len=0 new=1/0.400000006 null=1/0.400000006 run=1 PASS
ILLEGAL yellow len=1 new=1/0.400000006 null=1/0.400000006 run=1 PASS
ILLEGAL yellow len=2 new=1/0.400000006 null=1/0.400000006 run=1 PASS
ILLEGAL yellow len=4 new=1/0.400000006 null=1/0.400000006 run=1 PASS
ILLEGAL yellow len=5 new=1/0.400000006 null=1/0.400000006 run=1 PASS
ILLEGAL yellow len=6 new=1/0.400000006 null=1/0.400000006 run=1 PASS
ILLEGAL yellow len=12 new=1/0.400000006 null=1/0.400000006 run=1 PASS
EMPTY yellow new=1/0.400000006 null=1/0.400000006 run=1 PASS
RUNFAIL yellow new=1/0.400000006 null=1/0.400000006 run=1 PASS
NULL gray color=3 conf=0.5 calls=0
LEGAL gray max0 new=0/0.949999988 old=0/0.949999988 run=1 PASS
LEGAL gray max1 new=2/0.949999988 old=2/0.949999988 run=1 PASS
LEGAL gray max2 new=1/0.949999988 old=1/0.949999988 run=1 PASS
LEGAL gray lowconf new=3/0.5 old=3/0.5 run=1 PASS
ILLEGAL gray len=0 new=3/0.5 null=3/0.5 run=1 PASS
ILLEGAL gray len=1 new=3/0.5 null=3/0.5 run=1 PASS
ILLEGAL gray len=2 new=3/0.5 null=3/0.5 run=1 PASS
ILLEGAL gray len=4 new=3/0.5 null=3/0.5 run=1 PASS
ILLEGAL gray len=5 new=3/0.5 null=3/0.5 run=1 PASS
ILLEGAL gray len=6 new=3/0.5 null=3/0.5 run=1 PASS
ILLEGAL gray len=12 new=3/0.5 null=3/0.5 run=1 PASS
EMPTY gray new=3/0.5 null=3/0.5 run=1 PASS
RUNFAIL gray new=3/0.5 null=3/0.5 run=1 PASS
COUNT 52 FAILS 0
```

长度 5 的缓冲首值是 0.99。若主体仍读前三个，绿图会得到蓝且置信度 0.95 左右。实际绿图非法长度得到绿、置信度 0.4，与 `cls_` 为空相同。`run` 返回 false 时即使 outs 里放了长度 3 的高置信蓝，结果也与空引擎相同。每次有 `cls_` 的调用都 `run` 了一次，收到的是 96×96、`CV_8UC3`。

合法长度 3：下标 0/1/2 分别得到蓝/绿/黄，置信度 0.95，与旧主体一致。灰图低置信走到白、0.5；饱和图低置信仍保留 map3 的蓝、0.5。两边一致，这是原文里“低饱和才改白/黑”的行为，不是新 bug。

## 未覆盖

mock 不是 AX/ORT。没有 NPU2，没有 padding 字节，没有视频。线上预处理策略没有改。
