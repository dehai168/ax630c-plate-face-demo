#!/usr/bin/env python3
"""
Generate province-balanced synthetic 160x48 Chinese license plate crops for v2.7 OCR fine-tuning.
Covers all 31 provinces + 澳/港/使/警/学/挂 + 8-char Green/YellowGreen(D/F) + double-layer merged yellow + repeated digits.
"""
import argparse
import json
import os
import random
import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

PROVINCES = list("京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼")
LETTERS = list("ABCDEFGHJKLMNPQRSTUVWXYZ")
DIGITS = list("0123456789")
ALNUM = LETTERS + DIGITS
NEV_END = ["D", "F"]

BG_COLORS = {
    "blue": ((28, 72, 168), (245, 248, 255)),      # BGR bg, fg
    "yellow": ((20, 175, 235), (20, 20, 20)),
    "green": ((95, 195, 110), (20, 20, 20)),
    "white": ((235, 238, 240), (20, 20, 20)),
    "black": ((28, 28, 32), (240, 242, 245)),
}


def find_font():
    for p in [
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    ]:
        if os.path.exists(p):
            return p
    raise FileNotFoundError("No Chinese font found")


def random_plate(prov, rng):
    r = rng.random()
    if prov == "粤" and r < 0.08:
        suf = rng.choice(["港", "澳"])
        body = "".join(rng.choice(ALNUM) for _ in range(4))
        return f"粤Z{body}{suf}", "black", "single"
    if r < 0.06:
        # 8-char large NE (YellowGreen D/F)
        d5 = "".join(rng.choice(DIGITS) for _ in range(5))
        if rng.random() < 0.25:
            ch = rng.choice(["0", "7", "8", "9"])
            idx = rng.randint(0, 1)
            d5 = d5[:idx] + ch * 4 + d5[idx + 4 :]
            d5 = d5[:5]
        return f"{prov}{rng.choice(LETTERS)}{d5}{rng.choice(NEV_END)}", "yellowgreen", "single"
    if r < 0.14:
        # 8-char small NE (Green)
        p3 = rng.choice(NEV_END)
        p4 = rng.choice(ALNUM)
        d4 = "".join(rng.choice(DIGITS) for _ in range(4))
        return f"{prov}{rng.choice(LETTERS)}{p3}{p4}{d4}", "green", "single"
    if r < 0.20:
        # Police white
        c2 = "0" if rng.random() < 0.15 else rng.choice(LETTERS)
        body = "".join(rng.choice(DIGITS) for _ in range(4))
        if rng.random() < 0.25:
            ch = rng.choice(DIGITS)
            body = ch * 4
        return f"{prov}{c2}{body}警", "white", "single"
    if r < 0.26:
        # Coach yellow
        body = "".join(rng.choice(ALNUM if i < 2 else DIGITS) for i in range(4))
        return f"{prov}{rng.choice(LETTERS)}{body}学", "yellow", "single"
    if r < 0.34:
        # Trailer yellow (double or single)
        body = "".join(rng.choice(ALNUM if i < 2 else DIGITS) for i in range(4))
        layer = "double" if rng.random() < 0.65 else "single"
        return f"{prov}{rng.choice(LETTERS)}{body}挂", "yellow", layer
    if r < 0.42:
        # Double yellow ordinary
        body = "".join(rng.choice(ALNUM if i < 1 else DIGITS) for i in range(5))
        if rng.random() < 0.2:
            body = rng.choice(["0", "8"]) * 5
        return f"{prov}{rng.choice(LETTERS)}{body}", "yellow", "double"
    # Ordinary 7-char blue or yellow
    col = "yellow" if rng.random() < 0.30 else "blue"
    n_let = rng.randint(0, 2)
    chars = [rng.choice(DIGITS) for _ in range(5)]
    for pos in rng.sample(range(5), n_let):
        chars[pos] = rng.choice(LETTERS)
    return f"{prov}{rng.choice(LETTERS)}{''.join(chars)}", col, "single"


def render_plate(text, color_key, layer, font_path, rng):
    if layer == "double":
        # Render as 2-line 220x110 then apply merge_double (matching plate_rec.cpp)
        W, H = 220, 110
        bg, fg = BG_COLORS["yellow"]
        img = Image.new("RGB", (W, H), (bg[2], bg[1], bg[0]))
        draw = ImageDraw.Draw(img)
        f_top = ImageFont.truetype(font_path, 38)
        f_bot = ImageFont.truetype(font_path, 54)
        top_txt = text[:2]
        bot_txt = text[2:]
        draw.text((52, 4), top_txt, font=f_top, fill=(fg[2], fg[1], fg[0]))
        draw.text((16, 46), bot_txt, font=f_bot, fill=(fg[2], fg[1], fg[0]))
        bgr = cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
        # Apply merge_double
        h, w = bgr.shape[:2]
        up = bgr[0 : max(1, h * 5 // 12), :]
        down = bgr[h // 3 :, :]
        up = cv2.resize(up, (down.shape[1], down.shape[0]))
        bgr = cv2.hconcat([up, down])
    else:
        W = 300 if len(text) == 8 else 260
        H = 82
        if color_key == "yellowgreen":
            bgr_arr = np.zeros((H, W, 3), dtype=np.uint8)
            split = int(W * 0.28)
            bgr_arr[:, :split] = BG_COLORS["yellow"][0]
            bgr_arr[:, split:] = BG_COLORS["green"][0]
            fg = (20, 20, 20)
            img = Image.fromarray(cv2.cvtColor(bgr_arr, cv2.COLOR_BGR2RGB))
        else:
            bg, fg = BG_COLORS[color_key]
            img = Image.new("RGB", (W, H), (bg[2], bg[1], bg[0]))
        draw = ImageDraw.Draw(img)
        fnt = ImageFont.truetype(font_path, 52)
        x = 10
        for i, ch in enumerate(text):
            fill = (200, 30, 30) if ch in ("警", "使", "领") and color_key in ("white", "black") else (fg[2], fg[1], fg[0])
            draw.text((x, 12), ch, font=fnt, fill=fill)
            x += (34 if len(text) == 8 else 35)
            if i == 1:
                x += 8
        bgr = cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)

    # Augment: perspective/affine jitter, brightness/contrast, blur, noise, JPEG compression
    h, w = bgr.shape[:2]
    pts1 = np.float32([[0, 0], [w, 0], [w, h], [0, h]])
    dx, dy = w * 0.04, h * 0.07
    pts2 = np.float32([
        [rng.uniform(-dx, dx), rng.uniform(-dy, dy)],
        [w + rng.uniform(-dx, dx), rng.uniform(-dy, dy)],
        [w + rng.uniform(-dx, dx), h + rng.uniform(-dy, dy)],
        [rng.uniform(-dx, dx), h + rng.uniform(-dy, dy)],
    ])
    M = cv2.getPerspectiveTransform(pts1, pts2)
    bgr = cv2.warpPerspective(bgr, M, (w, h), borderMode=cv2.BORDER_REPLICATE)

    alpha = rng.uniform(0.65, 1.35)
    beta = rng.uniform(-25, 25)
    bgr = np.clip(bgr.astype(np.float32) * alpha + beta, 0, 255).astype(np.uint8)
    if rng.random() < 0.45:
        k = rng.choice([3, 5])
        bgr = cv2.GaussianBlur(bgr, (k, k), rng.uniform(0.3, 1.1))
    if rng.random() < 0.4:
        noise = np.random.normal(0, rng.uniform(2, 7), bgr.shape).astype(np.float32)
        bgr = np.clip(bgr.astype(np.float32) + noise, 0, 255).astype(np.uint8)

    # Resize to 160x48 using exact plate_rec.cpp aspect-preserving right-pad(128)
    TH, TW = 48, 160
    rw = min(TW, max(1, int(np.ceil(TH * (w / h)))))
    canvas = np.full((TH, TW, 3), 128, dtype=np.uint8)
    canvas[:, :rw] = cv2.resize(bgr, (rw, TH))
    q = rng.randint(45, 95)
    _, buf = cv2.imencode(".jpg", canvas, [int(cv2.IMWRITE_JPEG_QUALITY), q])
    return cv2.imdecode(buf, cv2.IMREAD_COLOR)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out_dir", required=True)
    ap.add_argument("--per_prov", type=int, default=200)
    ap.add_argument("--seed", type=int, default=20261127)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    np.random.seed(args.seed)
    os.makedirs(os.path.join(args.out_dir, "imgs"), exist_ok=True)
    font_path = find_font()

    manifest = []
    idx = 0
    for prov in PROVINCES:
        for _ in range(args.per_prov):
            text, col, layer = random_plate(prov, rng)
            im = render_plate(text, col, layer, font_path, rng)
            rel = f"imgs/syn_{idx:05d}.jpg"
            cv2.imwrite(os.path.join(args.out_dir, rel), im)
            manifest.append({"path": rel, "text": text, "color": col, "layer": layer, "src": "synth"})
            idx += 1
    with open(os.path.join(args.out_dir, "synth_manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False)
    print(f"Generated {len(manifest)} balanced synthetic plates ({args.per_prov}/prov) -> {args.out_dir}")


if __name__ == "__main__":
    main()
