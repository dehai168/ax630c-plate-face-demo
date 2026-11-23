# 离线几何对照结果：依赖缺失，C++ 未跑

合同：`/cursor/stores/self/v25-c-static-audit/05-geometry-contract.md`（对照开始前写入）。

## 基线

- 分支 `cursor/v25-c-integration-audit-50d6`
- 提交仍是 `2241e28c110f8a10b282619459743964e8b79d4e`
- 工作树干净。六文件 SHA 未变。
- 未改线上代码、模型、版本、真值。未合并，未部署，未开 PR。
- 未写 `if (layer==double)`。未启用 R84。

## 夹具身份（通过，这不是几何门）

包 `/cursor/stores/bot/v25-c-preprocess-fixtures/preprocess_fixtures.tar.gz`：1677077 字节，SHA256 `a8df601e060874357cad83f947abcd8a71361a2e7816811de0b9b6a4122746d9`，与交接一致。

`ARCHIVE_MANIFEST.json` 记 135 个成员。tar 内 135 个文件。`MEMBER_SHA256SUMS.txt` 列出其中 134 个，全部比对一致；未列入的是这份校验文件自己。包内没有 onnx、pt、axmodel。

`FIXTURE_LEDGER.json`：42 个 VALID，double 21，single 21，SU 0，失败 0。manifest SHA `db2c85bd16fe777cd39bdff8d1fdf96f5e9ade4ce263bdc35c510becff8435e7`。42 个 `source_SHA256_actual` 与 manifest 相同，且互不相同，来源都是 special。每个 `rectified_bgr.png` 和 `expected96_bgr.png` 的字节 SHA 与对应 `meta.json` 一致。`trainer_meta.rectified_is` 为 `post_points_warp_pre_double_stitch`。账本写明 `python_cpp_consistency_claimed: false`、`cpp_not_run: true`。生成夹具时的 OpenCV 是 5.0.0。

`plate_optim_common.py` 记录 SHA `8cd6a38344bb843a93f07401e6976cd40023ad67049c40693e84d9ccfe30b1cf`。本机没有该文件，也没有重跑 reference。

## 现有 helper（只读，未改编）

`src/plate_rec.cpp` 第 142–150 行，原文 SHA256 `2906c185fc124ee76f905f9f208a418f6719b925b92eed91a330efe341560c09`：

```
cv::Mat PlateRecognizer::merge_double(const cv::Mat& img) {
    int h = img.rows, w = img.cols;
    cv::Mat up = img(cv::Rect(0, 0, w, std::max(1, h * 5 / 12))).clone();
    cv::Mat down = img(cv::Rect(0, h / 3, w, h - h / 3)).clone();
    cv::resize(up, up, down.size());
    cv::Mat m;
    cv::hconcat(up, down, m);
    return m;
}
```

没有做去掉成员限定符的 adapter，因为编译依赖不存在，不能产生一份可链接的副本。没有第二套几何实现。

颜色 96 的线上调用仍是 `cv::resize` 默认插值（`prepare_color5_input` 与旧 `classify_color`）。归一化公式在 `src/infer/engine_ort.cpp`：BGR、不交换通道、`(x-0)/255`、NCHW。该函数在 ORT 的 `run` 里，不能脱离 ORT 头文件单独链接。按合同，F32 标为未测，不另造容差。

## 精确缺口（几何门未跑）

已有：`g++` 13.3.0。

没有：

- `/usr/include/opencv4`、`/usr/include/opencv2`
- `/usr/lib/x86_64-linux-gnu/libopencv_core.so`
- 包 `libopencv-dev`、`libopencv-core-dev`（`dpkg-query`：no packages found）
- `pkg-config opencv4` 不存在
- Python 模块 `cv2` 也不存在

因此没有编译 harness，没有读 PNG 像素，没有 U8 逐像素比较，没有负控，没有 CHW float32。未安装任何依赖，未用 Python 代替 C++。

| 门 | 结果 |
|---|---|
| 夹具字节身份 | 通过 |
| 42 张 U8 与 expected96 逐像素 | 未跑 |
| 归一化 F32 bitwise / ULP | 未测 |
| 单层 / 双层分项像素差 | 无 |
| 几何通过 | 否 |

## 仍未闭合

即使以后 42 张 U8 全相等，线上仍没有 source 运行时来选择是否拼接。本次不把离线 `layer` 写成预测层。双层颜色策略不改。R84 不启用。`9d41…` 未绑定到这次几何。

下一就绪条件：环境里已经有 C++ OpenCV 头文件和 `core` / `imgproc` / `imgcodecs`，再只编译从第 142–150 行机械抽出的 helper。夹具侧记录的是 OpenCV 5.0.0；版本不同时只许记录差异，不许改插值去凑通过。
