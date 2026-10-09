#!/usr/bin/env bash
# 交叉编译板端依赖：FFmpeg(LGPL, 动态库, 仅内置编解码器) + OpenCV(静态, 带 FFmpeg videoio)
# 用法: TOOLCHAIN_DIR=/opt/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu \
#       FFMPEG_SRC=/path/ffmpeg-6.1.2 OPENCV_SRC=/path/opencv-4.10.0 ./scripts/build_3rdparty_aarch64.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${TOOLCHAIN_DIR:?set TOOLCHAIN_DIR to gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu}"
: "${FFMPEG_SRC:?set FFMPEG_SRC (ffmpeg-6.1.x source dir)}"
: "${OPENCV_SRC:?set OPENCV_SRC (opencv-4.10.0 source dir)}"
PREFIX=${PREFIX:-$ROOT/3rdparty/aarch64}
JOBS=${JOBS:-$(nproc)}
CROSS=$TOOLCHAIN_DIR/bin/aarch64-none-linux-gnu-
export PATH=$TOOLCHAIN_DIR/bin:$PATH
mkdir -p "$PREFIX"

if [ "${SKIP_FFMPEG:-0}" != "1" ]; then
echo "==> FFmpeg -> $PREFIX"
BUILD_FF=$ROOT/build/ffmpeg-aarch64; rm -rf "$BUILD_FF"; mkdir -p "$BUILD_FF"; cd "$BUILD_FF"
"$FFMPEG_SRC"/configure --prefix="$PREFIX" --enable-cross-compile --cross-prefix="$CROSS" \
  --arch=aarch64 --target-os=linux --enable-shared --disable-static --disable-programs --disable-doc \
  --disable-autodetect --disable-everything --disable-network --disable-avdevice --disable-postproc \
  --enable-avcodec --enable-avformat --enable-swscale --enable-swresample --enable-avutil \
  --enable-decoder=h264,hevc,mpeg4,mjpeg,rawvideo --enable-encoder=mpeg4,mjpeg,rawvideo \
  --enable-parser=h264,hevc,mpeg4video,mjpeg --enable-demuxer=mov,avi,matroska,h264,hevc,mjpeg \
  --enable-muxer=mp4,mov,avi,matroska --enable-protocol=file --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb \
  --enable-pic --extra-ldflags="-Wl,-rpath,\$\$ORIGIN"
make -j"$JOBS" && make install
fi

echo "==> OpenCV -> $PREFIX"
BUILD_CV=$ROOT/build/opencv-aarch64; rm -rf "$BUILD_CV"; mkdir -p "$BUILD_CV"; cd "$BUILD_CV"
export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig PKG_CONFIG_LIBDIR=$PREFIX/lib/pkgconfig
cmake "$OPENCV_SRC" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/aarch64-none-linux-gnu.toolchain.cmake" -DTOOLCHAIN_DIR="$TOOLCHAIN_DIR" \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_LIST=core,imgproc,imgcodecs,videoio -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF \
  -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF -DBUILD_opencv_python3=OFF \
  -DBUILD_ZLIB=ON -DBUILD_JPEG=ON -DBUILD_PNG=ON -DBUILD_TIFF=OFF -DBUILD_WEBP=OFF -DBUILD_OPENJPEG=OFF -DWITH_OPENJPEG=OFF \
  -DWITH_JASPER=OFF -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENEXR=OFF -DWITH_GTK=OFF -DWITH_QT=OFF -DWITH_V4L=OFF \
  -DWITH_GSTREAMER=OFF -DWITH_OPENCL=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_PROTOBUF=OFF -DWITH_EIGEN=OFF \
  -DWITH_ADE=OFF -DWITH_FFMPEG=ON -DOPENCV_FFMPEG_USE_FIND_PACKAGE=OFF -DOPENCV_GENERATE_PKGCONFIG=OFF -DCV_TRACE=OFF \
  -DENABLE_NEON=ON -DCPU_BASELINE=NEON
ninja -j"$JOBS" && ninja install
echo "OK: $PREFIX"
