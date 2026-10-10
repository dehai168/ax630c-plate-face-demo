// v2.3 规则单元测试：车牌格式 / 大型新能源 / 黄绿颜色
#include <cstdio>
#include "filters.h"
#include "plate_rec.h"
using namespace pfd;
static int fails = 0, total = 0;
#define CHECK(cond) do { ++total; if (!(cond)) { ++fails; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
static const char* cn(int c) { return plate_color_cn(c); }
int main() {
    PlateRecConfig cfg;
    struct { const char* t; bool fmt; bool large; int cls; int expect; } cases[] = {   // cls: 分类网络颜色
        {"粤BD12345", true, false, PC_GREEN, PC_GREEN},          // 小型新能源（第 3 位动力字母）
        {"粤BF12345", true, false, PC_YELLOW, PC_GREEN},
        {"粤B12345D", true, true, PC_GREEN, PC_YELLOWGREEN},      // 大型纯电
        {"粤B12345F", true, true, PC_YELLOW, PC_YELLOWGREEN},     // 大型非纯电
        {"粤B12345K", true, true, PC_GREEN, PC_YELLOWGREEN},      // 扩展字母
        {"粤B1234AD", true, true, PC_GREEN, PC_YELLOWGREEN},      // 序号含字母
        {"粤B12345D", true, true, PC_BLUE, PC_GREEN},             // 分类为蓝 -> 不判黄绿（按 8 位规则为绿）
        {"粤B12345L", false, false, PC_GREEN, PC_GREEN},          // 末位非动力字母 -> 格式不合法
        {"渝AF16030", true, false, PC_GREEN, PC_GREEN},           // 实测视频中的小型新能源
        {"渝ADW5289", true, false, PC_GREEN, PC_GREEN},
        {"渝A12345", true, false, PC_BLUE, PC_BLUE},              // 7 位普通
        {"渝A12345", true, false, PC_YELLOWGREEN, PC_GREEN},      // 投票色为黄绿但号码不是大型新能源 -> 退回绿
        {"粤BD1234D", true, true, PC_GREEN, PC_YELLOWGREEN},      // 末位为动力字母按大型处理
    };
    for (auto& c : cases) {
        bool f = strict_plate_format(c.t, 3), l = is_large_ne_plate(c.t);
        int n8 = (int)utf8_chars(c.t).size() == 8 && c.fmt;
        int col = n8 ? PC_GREEN : c.cls;   // 管线: 8 位合法号码先按规则给绿
        int r = resolve_plate_color(c.t, col, c.cls == PC_YELLOWGREEN ? PC_GREEN : c.cls, cfg);
        if (c.cls == PC_YELLOWGREEN) r = resolve_plate_color(c.t, PC_YELLOWGREEN, PC_GREEN, cfg);
        printf("%-12s fmt=%d large=%d cls=%s -> %s\n", c.t, f, l, cn(c.cls), cn(r));
        CHECK(f == c.fmt); CHECK(l == c.large); CHECK(r == c.expect);
    }
    cfg.yellowgreen_rule = false;
    CHECK(resolve_plate_color("粤B12345D", PC_GREEN, PC_GREEN, cfg) == PC_GREEN);
    cfg.yellowgreen_rule = true; cfg.yellowgreen_letters = "DF";
    CHECK(resolve_plate_color("粤B12345K", PC_GREEN, PC_GREEN, cfg) == PC_GREEN);
    CHECK(resolve_plate_color("粤B12345D", PC_GREEN, PC_YELLOW, cfg) == PC_YELLOWGREEN);
    printf("%d/%d passed\n", total - fails, total);
    return fails ? 1 : 0;
}
