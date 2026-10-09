#include "engine.h"
#include <cstdio>
#include <onnxruntime_cxx_api.h>

namespace pfd {

class OrtEngine : public InferEngine {
public:
    bool load(const std::string& path, const NormConfig& norm) override {
        norm_ = norm;
        try {
            Ort::SessionOptions so;
            so.SetIntraOpNumThreads(2);
            so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            session_ = std::make_unique<Ort::Session>(env(), path.c_str(), so);
            Ort::AllocatorWithDefaultOptions alloc;
            in_name_ = session_->GetInputNameAllocated(0, alloc).get();
            auto shp = session_->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
            if (shp.size() != 4) { fprintf(stderr, "[ORT] %s: input must be NCHW\n", path.c_str()); return false; }
            in_h_ = (int)shp[2]; in_w_ = (int)shp[3];
            for (size_t i = 0; i < session_->GetOutputCount(); ++i)
                out_names_.push_back(session_->GetOutputNameAllocated(i, alloc).get());
            input_.resize((size_t)3 * in_h_ * in_w_);
        } catch (const std::exception& e) {
            fprintf(stderr, "[ORT] load %s failed: %s\n", path.c_str(), e.what());
            return false;
        }
        return true;
    }
    int input_h() const override { return in_h_; }
    int input_w() const override { return in_w_; }
    std::string backend() const override { return "onnxruntime"; }

    bool run(const cv::Mat& img, std::vector<Tensor>& outputs) override {
        if (img.rows != in_h_ || img.cols != in_w_ || img.type() != CV_8UC3) return false;
        const int hw = in_h_ * in_w_;
        float* c0 = input_.data(); float* c1 = c0 + hw; float* c2 = c1 + hw;
        float* dst[3] = {c0, c1, c2};
        // BGR HWC u8 -> NCHW float, (x-mean)/std, 可选 BGR->RGB
        for (int y = 0; y < in_h_; ++y) {
            const uint8_t* p = img.ptr<uint8_t>(y);
            for (int x = 0; x < in_w_; ++x, p += 3) {
                int idx = y * in_w_ + x;
                for (int c = 0; c < 3; ++c) {
                    int src_c = norm_.to_rgb ? 2 - c : c;
                    dst[c][idx] = ((float)p[src_c] - norm_.mean[c]) / norm_.std[c];
                }
            }
        }
        std::array<int64_t, 4> ishape{1, 3, in_h_, in_w_};
        auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value in = Ort::Value::CreateTensor<float>(mem, input_.data(), input_.size(), ishape.data(), 4);
        std::vector<const char*> on; for (auto& s : out_names_) on.push_back(s.c_str());
        const char* inn = in_name_.c_str();
        try {
            outs_ = session_->Run(Ort::RunOptions{nullptr}, &inn, &in, 1, on.data(), on.size());
        } catch (const std::exception& e) {
            fprintf(stderr, "[ORT] run failed: %s\n", e.what());
            return false;
        }
        outputs.clear();
        for (auto& v : outs_) {
            Tensor t;
            for (auto d : v.GetTensorTypeAndShapeInfo().GetShape()) t.shape.push_back((int)d);
            t.data = v.GetTensorData<float>();
            outputs.push_back(t);
        }
        return true;
    }

private:
    static Ort::Env& env() { static Ort::Env e(ORT_LOGGING_LEVEL_WARNING, "pfd"); return e; }
    std::unique_ptr<Ort::Session> session_;
    std::string in_name_;
    std::vector<std::string> out_names_;
    std::vector<float> input_;
    std::vector<Ort::Value> outs_;
    NormConfig norm_;
    int in_h_ = 0, in_w_ = 0;
};

std::unique_ptr<InferEngine> create_engine() { return std::make_unique<OrtEngine>(); }
bool backend_global_init() { return true; }
void backend_global_deinit() {}

}  // namespace pfd
