"""CCPD 文件名解析（https://github.com/detectRecog/CCPD, MIT）"""
import os
PROVINCES = ["皖", "沪", "津", "渝", "冀", "晋", "蒙", "辽", "吉", "黑", "苏", "浙", "京", "闽", "赣", "鲁", "豫", "鄂", "湘", "粤", "桂", "琼", "川", "贵", "云", "藏", "陕", "甘", "青", "宁", "新", "警", "学", "O"]
ALPHABETS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'O']
ADS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'O']

def parse(path):
    name = os.path.splitext(os.path.basename(path))[0].replace(',', '&')  # 有的镜像把 & 换成了 ,
    f = name.split('-')
    if len(f) < 7:
        return None
    (x1, y1), (x2, y2) = [tuple(map(int, p.split('&'))) for p in f[2].split('_')]
    verts = [tuple(map(int, p.split('&'))) for p in f[3].split('_')]  # 右下, 左下, 左上, 右上
    idx = list(map(int, f[4].split('_')))
    text = PROVINCES[idx[0]] + ALPHABETS[idx[1]] + ''.join(ADS[i] for i in idx[2:])
    tl, tr, br, bl = verts[2], verts[3], verts[0], verts[1]
    return dict(text=text, box=(x1, y1, x2, y2), corners=[tl, tr, br, bl],
                brightness=int(f[5]), blur=int(f[6].split('_')[0]),
                subset=('ccpd_' + f[6].split('_ccpd_')[1].rsplit('_', 1)[0]) if '_ccpd_' in f[6] else '')
