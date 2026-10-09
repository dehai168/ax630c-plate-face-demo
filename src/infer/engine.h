// 推理后端抽象层：板端 AXEngine(.axmodel)，PC 端 ONNX Runtime(.onnx)，前后处理完全共用。
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

namespace pfd {

struct Tensor {
    std::vector<int> shape;   // 例如 [1,45,48,80] (NCHW) 或 [1,48,80,45] (NHWC)
    const float* data = nullptr;
    size_t size() const { size_t n = 1; for (int s : shape) n *= (size_t)s; return n; }
};

// ORT 后端: 全部前处理在 CPU 做；AX 后端: mean/std 已由 Pulsar2 固化进 axmodel，只使用 to_rgb（Pulsar2 不做通道重排）
struct NormConfig {
    bool to_rgb = false;            // 输入统一是 BGR uint8 NHWC，to_rgb=true 时 ORT 后端会交换通道
    float mean[3] = {0.f, 0.f, 0.f};
    float std[3] = {255.f, 255.f, 255.f};
};

class InferEngine {
public:
    virtual ~InferEngine() = default;
    virtual bool load(const std::string& path, const NormConfig& norm) = 0;
    virtual int input_h() const = 0;
    virtual int input_w() const = 0;
    // img: BGR, uint8, 连续内存，尺寸必须等于 input_w x input_h
    virtual bool run(const cv::Mat& img, std::vector<Tensor>& outputs) = 0;
    virtual std::string backend() const = 0;
};

// 根据编译选项创建后端（PFD_BACKEND_AX / PFD_BACKEND_ORT）
std::unique_ptr<InferEngine> create_engine();
// 板端 AX 运行时初始化/反初始化（ORT 下为空操作）
bool backend_global_init();
void backend_global_deinit();

}  // namespace pfd
