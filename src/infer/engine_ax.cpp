// 爱芯 AX620E (AX630C/AX620Q) AXEngine 后端，参考 AXERA-TECH/ax-samples examples/ax620e (BSD-3-Clause)
#include "engine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <opencv2/imgproc.hpp>
#include <ax_sys_api.h>
#include <ax_engine_api.h>

namespace pfd {

static const char* kCmmToken = "plate_face_demo";
static int g_vnpu_mode = 0;   // 0: AX_ENGINE_VIRTUAL_NPU_DISABLE

class AxEngine : public InferEngine {
public:
    ~AxEngine() override { release(); }

    bool load(const std::string& path, const NormConfig& norm) override {
        norm_ = norm;
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) { fprintf(stderr, "[AX] open %s failed\n", path.c_str()); return false; }
        size_t n = (size_t)f.tellg(); f.seekg(0);
        model_.resize(n);
        f.read(model_.data(), n);
        int ret = AX_ENGINE_CreateHandle(&handle_, model_.data(), (AX_U32)n);
        if (ret) { fprintf(stderr, "[AX] CreateHandle(%s) ret=0x%x\n", path.c_str(), ret); return false; }
        ret = AX_ENGINE_CreateContext(handle_);
        if (ret) { fprintf(stderr, "[AX] CreateContext ret=0x%x\n", ret); return false; }
        ret = AX_ENGINE_GetIOInfo(handle_, &info_);
        if (ret) { fprintf(stderr, "[AX] GetIOInfo ret=0x%x\n", ret); return false; }
        if (info_->nInputSize != 1) { fprintf(stderr, "[AX] only 1 input supported\n"); return false; }
        auto& in = info_->pInputs[0];
        // 期望 NHWC uint8: [1,H,W,3]
        if (in.nShapeSize != 4) { fprintf(stderr, "[AX] unexpected input rank %d\n", (int)in.nShapeSize); return false; }
        in_h_ = in.pShape[1]; in_w_ = in.pShape[2];
        if (in.pShape[3] != 3 || in.nSize != (AX_U32)(in_h_ * in_w_ * 3)) {
            fprintf(stderr, "[AX] %s: input must be NHWC u8 x3 (shape %dx%dx%dx%d, size %u). "
                            "请用 scripts/pulsar2_configs 下的配置重新编译 (src_dtype=U8, src_layout=NHWC, src_format=BGR)\n",
                    path.c_str(), in.pShape[0], in.pShape[1], in.pShape[2], in.pShape[3], in.nSize);
            return false;
        }
        memset(&io_, 0, sizeof(io_));
        io_.nInputSize = 1;
        io_.pInputs = new AX_ENGINE_IO_BUFFER_T[1];
        memset(io_.pInputs, 0, sizeof(AX_ENGINE_IO_BUFFER_T));
        io_.pInputs[0].nSize = in.nSize;
        ret = AX_SYS_MemAllocCached((AX_U64*)&io_.pInputs[0].phyAddr, &io_.pInputs[0].pVirAddr, in.nSize, 128, (const AX_S8*)kCmmToken);
        if (ret) { fprintf(stderr, "[AX] alloc input ret=0x%x\n", ret); return false; }
        io_.nOutputSize = info_->nOutputSize;
        io_.pOutputs = new AX_ENGINE_IO_BUFFER_T[io_.nOutputSize];
        memset(io_.pOutputs, 0, sizeof(AX_ENGINE_IO_BUFFER_T) * io_.nOutputSize);
        for (AX_U32 i = 0; i < io_.nOutputSize; ++i) {
            auto& o = info_->pOutputs[i];
            if (o.eDataType != AX_ENGINE_DT_FLOAT32) {
                fprintf(stderr, "[AX] output %s is not float32, 请在 Pulsar2 配置里保持输出为 FP32\n", o.pName);
                return false;
            }
            io_.pOutputs[i].nSize = o.nSize;
            ret = AX_SYS_MemAllocCached((AX_U64*)&io_.pOutputs[i].phyAddr, &io_.pOutputs[i].pVirAddr, o.nSize, 128, (const AX_S8*)kCmmToken);
            if (ret) { fprintf(stderr, "[AX] alloc output ret=0x%x\n", ret); return false; }
            Tensor t;
            for (AX_U8 s = 0; s < o.nShapeSize; ++s) t.shape.push_back(o.pShape[s]);
            t.data = (const float*)io_.pOutputs[i].pVirAddr;
            outs_.push_back(t);
        }
        fprintf(stdout, "[AX] loaded %s input %dx%d, %u outputs\n", path.c_str(), in_w_, in_h_, io_.nOutputSize);
        return true;
    }
    int input_h() const override { return in_h_; }
    int input_w() const override { return in_w_; }
    std::string backend() const override { return "axengine"; }

    bool run(const cv::Mat& img, std::vector<Tensor>& outputs) override {
        if (img.rows != in_h_ || img.cols != in_w_ || img.type() != CV_8UC3) return false;
        auto& ib = io_.pInputs[0];
        // Pulsar2 不会在模型内做 RGB<->BGR 重排：需要 RGB 输入的模型(检测)在这里直接转换写进 CMM 输入缓冲
        cv::Mat dst(in_h_, in_w_, CV_8UC3, ib.pVirAddr);
        if (norm_.to_rgb) cv::cvtColor(img, dst, cv::COLOR_BGR2RGB);
        else img.copyTo(dst);
        AX_SYS_MflushCache(ib.phyAddr, ib.pVirAddr, ib.nSize);
        int ret = AX_ENGINE_RunSync(handle_, &io_);
        if (ret) { fprintf(stderr, "[AX] RunSync ret=0x%x\n", ret); return false; }
        for (AX_U32 i = 0; i < io_.nOutputSize; ++i)
            AX_SYS_MinvalidateCache(io_.pOutputs[i].phyAddr, io_.pOutputs[i].pVirAddr, io_.pOutputs[i].nSize);
        outputs = outs_;
        return true;
    }

private:
    void release() {
        if (io_.pInputs) {
            for (AX_U32 i = 0; i < io_.nInputSize; ++i)
                if (io_.pInputs[i].pVirAddr) AX_SYS_MemFree(io_.pInputs[i].phyAddr, io_.pInputs[i].pVirAddr);
            delete[] io_.pInputs; io_.pInputs = nullptr;
        }
        if (io_.pOutputs) {
            for (AX_U32 i = 0; i < io_.nOutputSize; ++i)
                if (io_.pOutputs[i].pVirAddr) AX_SYS_MemFree(io_.pOutputs[i].phyAddr, io_.pOutputs[i].pVirAddr);
            delete[] io_.pOutputs; io_.pOutputs = nullptr;
        }
        if (handle_) { AX_ENGINE_DestroyHandle(handle_); handle_ = nullptr; }
    }
    NormConfig norm_;
    std::vector<char> model_;
    AX_ENGINE_HANDLE handle_ = nullptr;
    AX_ENGINE_IO_INFO_T* info_ = nullptr;
    AX_ENGINE_IO_T io_{};
    std::vector<Tensor> outs_;
    int in_h_ = 0, in_w_ = 0;
};

std::unique_ptr<InferEngine> create_engine() { return std::make_unique<AxEngine>(); }

bool backend_global_init() {
    int ret = AX_SYS_Init();
    if (ret) { fprintf(stderr, "[AX] AX_SYS_Init ret=0x%x\n", ret); return false; }
    AX_ENGINE_NPU_ATTR_T attr;
    memset(&attr, 0, sizeof(attr));
    const char* v = getenv("PFD_VNPU");   // 可选: PFD_VNPU=1 开启虚拟 NPU（仅能跑 npu1 模型）
    g_vnpu_mode = v ? atoi(v) : 0;
    attr.eHardMode = g_vnpu_mode ? AX_ENGINE_VIRTUAL_NPU_ENABLE : AX_ENGINE_VIRTUAL_NPU_DISABLE;
    ret = AX_ENGINE_Init(&attr);
    if (ret) { fprintf(stderr, "[AX] AX_ENGINE_Init ret=0x%x\n", ret); return false; }
    fprintf(stdout, "[AX] engine version: %s, vnpu=%d\n", AX_ENGINE_GetVersion(), g_vnpu_mode);
    return true;
}
void backend_global_deinit() {
    AX_ENGINE_Deinit();
    AX_SYS_Deinit();
}

}  // namespace pfd
