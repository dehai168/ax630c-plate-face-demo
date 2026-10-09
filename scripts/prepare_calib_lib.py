"""prepare_calib.py / eval 共用的前处理（与 C++ 实现保持一致）"""
import cv2, numpy as np

def letterbox(img, W, H, pad):
    r = min(W / img.shape[1], H / img.shape[0])
    nw, nh = int(round(img.shape[1] * r)), int(round(img.shape[0] * r))
    out = np.full((H, W, 3), pad, np.uint8)
    px, py = (W - nw) // 2, (H - nh) // 2
    out[py:py + nh, px:px + nw] = cv2.resize(img, (nw, nh))
    return out

def rectify(img, corners, ex=0.02, ey=0.04):
    p = np.float32(corners)
    ux = ((p[1] - p[0]) + (p[2] - p[3])) / 2; uy = ((p[3] - p[0]) + (p[2] - p[1])) / 2
    s = np.float32([[-1, -1], [1, -1], [1, 1], [-1, 1]])
    p = p + ux[None] * (s[:, :1] * ex / 2) + uy[None] * (s[:, 1:] * ey / 2)
    w = int(max(np.linalg.norm(p[0] - p[1]), np.linalg.norm(p[2] - p[3])))
    h = int(max(np.linalg.norm(p[0] - p[3]), np.linalg.norm(p[1] - p[2])))
    M = cv2.getPerspectiveTransform(p, np.float32([[0, 0], [w, 0], [w, h], [0, h]]))
    return cv2.warpPerspective(img, M, (w, h), borderMode=cv2.BORDER_REPLICATE)

def rec_input(plate, H=48, W=160):
    rw = min(W, max(1, int(np.ceil(H * plate.shape[1] / plate.shape[0]))))
    out = np.full((H, W, 3), 128, np.uint8)
    out[:, :rw] = cv2.resize(plate, (rw, H))
    return out

