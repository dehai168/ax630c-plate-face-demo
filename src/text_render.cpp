#include "text_render.h"
#include <map>
#include <mutex>
#include <vector>
#include <opencv2/imgproc.hpp>
#include "glyph_atlas.h"

namespace pfd {

static std::vector<uint32_t> decode_utf8(const std::string& s) {
    std::vector<uint32_t> cps;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        uint32_t cp; int l;
        if (c < 0x80) { cp = c; l = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; l = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; l = 3; }
        else { cp = c & 0x07; l = 4; }
        for (int k = 1; k < l && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
        cps.push_back(cp); i += l;
    }
    return cps;
}

static int find_glyph(uint32_t cp) {
    static std::map<uint32_t, int> idx;
    static std::once_flag once;
    std::call_once(once, [] { for (int i = 0; i < kGlyphCount; ++i) idx[kGlyphs[i].cp] = i; });
    auto it = idx.find(cp);
    if (it != idx.end()) return it->second;
    auto q = idx.find((uint32_t)'?');
    return q != idx.end() ? q->second : 0;
}

struct ScaledAtlas { cv::Mat a; float s; };
static const ScaledAtlas& atlas_for(int height) {
    static std::map<int, ScaledAtlas> cache;
    auto it = cache.find(height);
    if (it != cache.end()) return it->second;
    cv::Mat base(kGlyphAtlasH, kGlyphAtlasW, CV_8UC1, (void*)kGlyphAtlas);
    float s = (float)height / kGlyphAtlasH;
    ScaledAtlas sa; sa.s = s;
    if (height == kGlyphAtlasH) sa.a = base.clone();
    else cv::resize(base, sa.a, cv::Size(std::max(1, (int)(kGlyphAtlasW * s)), height), 0, 0, cv::INTER_AREA);
    return cache.emplace(height, sa).first->second;
}

cv::Size text_size(const std::string& utf8, int height) {
    float s = (float)height / kGlyphAtlasH;
    int w = 0;
    for (auto cp : decode_utf8(utf8)) w += (int)(kGlyphs[find_glyph(cp)].w * s);
    return cv::Size(w, height);
}

cv::Size put_text(cv::Mat& img, const std::string& utf8, cv::Point org, int height, const cv::Scalar& color,
                  const cv::Scalar* bg, int pad) {
    const ScaledAtlas& sa = atlas_for(height);
    cv::Size sz = text_size(utf8, height);
    if (bg) {
        cv::Rect r(org.x - pad, org.y - pad, sz.width + 2 * pad, sz.height + 2 * pad);
        r &= cv::Rect(0, 0, img.cols, img.rows);
        if (r.area() > 0) img(r).setTo(*bg);
    }
    int x = org.x;
    for (auto cp : decode_utf8(utf8)) {
        const GlyphEntry& g = kGlyphs[find_glyph(cp)];
        int gx = (int)(g.x * sa.s), gw = (int)(g.w * sa.s);
        gx = std::min(gx, sa.a.cols - 1); gw = std::min(gw, sa.a.cols - gx);
        for (int yy = 0; yy < height; ++yy) {
            int py = org.y + yy;
            if (py < 0 || py >= img.rows) continue;
            const uint8_t* a = sa.a.ptr<uint8_t>(yy) + gx;
            uint8_t* d = img.ptr<uint8_t>(py);
            for (int xx = 0; xx < gw; ++xx) {
                int px = x + xx;
                if (px < 0 || px >= img.cols || a[xx] == 0) continue;
                float al = a[xx] / 255.f;
                for (int c = 0; c < 3; ++c) d[px * 3 + c] = (uint8_t)(d[px * 3 + c] * (1 - al) + color[c] * al);
            }
        }
        x += gw;
    }
    return sz;
}

}  // namespace pfd
