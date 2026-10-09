#!/usr/bin/env python3
"""把 demo 输出的 snap_dir/*.json 与 make_test_video.py 生成的 *_gt.json 对比（每段的车牌号 / 人脸 track 数）
用法: python check_against_gt.py --gt test_video_gt.json --snaps snaps/
"""
import argparse, glob, json, os
ap = argparse.ArgumentParser(); ap.add_argument('--gt', required=True); ap.add_argument('--snaps', required=True)
a = ap.parse_args()
gt = json.load(open(a.gt, encoding='utf-8'))
plates = [json.load(open(f, encoding='utf-8')) for f in sorted(glob.glob(os.path.join(a.snaps, 'plate_*.json')))]
faces = [json.load(open(f, encoding='utf-8')) for f in sorted(glob.glob(os.path.join(a.snaps, 'face_*.json')))]
ok = n = 0
for s in gt['segments']:
    lo, hi = s['start_frame'], s['end_frame']
    ps = [p for p in plates if lo <= p['best_frame'] <= hi]
    fs = [f for f in faces if lo <= f['best_frame'] <= hi]
    line = f"seg{s['index']:2d} {s['layout']:10s} frames[{lo}-{hi}] faces_tracks={len(fs):2d}"
    if s['plate_text']:
        n += 1
        texts = [f"#{p['track_id']}:{p['plate_text']}/{p['color_cn']}" for p in ps]
        hit = any(p['plate_text'] == s['plate_text'] for p in ps)
        ok += hit
        line += f"  GT {s['plate_text']}  pred {texts}  {'OK' if hit else 'MISS'}"
    print(line)
print(f'plate segments matched: {ok}/{n}')
