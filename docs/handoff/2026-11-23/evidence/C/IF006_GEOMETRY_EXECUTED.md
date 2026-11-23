# TRAIN42 几何对照已执行

`05-geometry-contract.md` SHA256 `fe95a91db06ee56f09f1fc6118a4a47e70fc1d114bb91ed5a3ae3af8d5c48aab`。
`06-geometry-result.md` SHA256 `0600dd260f3c2b8109911aac9f42c0834084c947668ac2141c5271e05fdd68e2`。
这两份没有改。06 里的未跑不是像素失败。环境合同是 `07-opencv-env-contract.md`。

## 仓库

提交仍是 `2241e28c110f8a10b282619459743964e8b79d4e`。工作树干净。六文件 SHA 未变。未改模型、版本、真值或 sidecar。未合并，未部署。线上是否拼接的策略没有改。R84 仍不启用。Python `cv2` 未安装，reference 脚本未重跑。

## 安装

源是已配置的 `http://archive.ubuntu.com/ubuntu` 与 `http://security.ubuntu.com/ubuntu`，`Signed-By` 为系统 Ubuntu keyring。没有加 PPA，没有关签名，没有 upgrade，没有删包。

`apt-get update`：2026-11-17T02:15:27Z 到 02:16:31Z，索引约 34.0 MB。

模拟安装后实际安装：`sudo apt-get install -y --no-install-recommends libopencv-core-dev libopencv-imgproc-dev libopencv-imgcodecs-dev`。

- 开始 2026-11-17T02:17:03Z，结束 02:17:08Z
- 下载 39.9 MB（39899310 字节），56 个新包，0 升级，0 删除
- apt 声明额外磁盘 156 MB。实测已用空间从 8270000128 增到 8425803776，增加 155803648 字节
- OpenCV 版本 `4.6.0+dfsg-13.1ubuntu1`（运行时 `cv::getVersionString()` 为 `4.6.0`）
- 夹具生成侧记录的是 OpenCV 5.0.0。没有为凑结果改插值

必要依赖含 imgcodecs 拉来的 gdal、hdf5、proj-data 等。没有装 `libopencv-dev`、GUI、Qt、VTK、Python cv2、ORT 或 NPU。

## helper

原文 `src/plate_rec.cpp` 第 142–150 行，SHA256 `2906c185fc124ee76f905f9f208a418f6719b925b92eed91a330efe341560c09`。adapter 只删掉 `PlateRecognizer::`。adapter SHA256 `c3a926fb1c7105513faed373ff71756e0f925156786c260aa3419e600d3906d3`。随后 `cv::resize` 到 96×96，使用默认插值，没有改 flag。

夹具包 SHA256 `a8df601e060874357cad83f947abcd8a71361a2e7816811de0b9b6a4122746d9`。42 张重新核对 PNG 字节：21 double、21 single、source 全是 special、SU 0。

编译：`g++ -std=c++14 -O2 -Wall -Wextra -I/usr/include/opencv4 -o /tmp/geom42/harness /tmp/geom42/harness.cpp -lopencv_core -lopencv_imgproc -lopencv_imgcodecs`，1.020 秒。

运行：`/tmp/geom42/harness /tmp/geom42/cases.json /cursor/stores/self/v25-c-static-audit/geometry-run`，32.721 秒。进程退出码 0。合计在 5 分钟内。

## U8 门：PASS

42/42 与 `expected96_bgr.png` 逐像素差为 0，max 0，MAE 0。输出 PNG 与期望 PNG 的文件 SHA 也 42/42 相同。分项：double 21/21，single 21/21。

样本 `double_00` 输出 PNG SHA256 `67af971c1b0515b3220916e165cb98938a4e3e29a16b3b41f86e74191098b970`。每张的 `cpp96_bgr.png`、`cpp96_bgr.raw`、`cpp96_chw_f32.raw` 在 `geometry-run/<id>/`。

负控只对 double：不拼接、直接把 rectified resize 到 96。21 张差异像素 9183 到 9216（满幅 9216），max abs 40 到 249。没有用这个结果改参数或插值。single 没有做这组负控。

## F32

独立界限：没有链接 `OrtEngine::run`。数值来自 `src/infer/engine_ort.cpp` 第 40–48 行的循环，颜色侧 `NormConfig` 默认 `to_rgb=false`、mean 0、std 255，即 BGR、NCHW、`u8/255`。这不是模型调用。

42/42 的 CHW float32 原始字节 SHA 与 meta 里的 `chw_f32_sha256` 逐字节相同。相对 expected PNG 再走同一公式，不等元素数 0，max abs 0，max ULP 0。U8 通过不是 F32 通过的替代；F32 是单独比过的。

## 未闭合

这次只说明：在这 42 张训练器截下的 rectified BGR 上，现有 `merge_double` 再默认 resize 96，与冻结的 Python expected96 逐像素一致。它不决定线上用什么层去选择拼接，也不启用 R84，也不覆盖 SU（该 manifest 里是 0）。
