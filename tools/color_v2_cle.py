#!/usr/bin/env python3
"""
Exact mathematical Cross-Layer Equalization (CLE) for canonical ColorV2 ONNX
(ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360).

Because ReLU, MaxPool, and GlobalAveragePool/ReduceMean are all positively
homogeneous (f(s * x) = s * f(x) for any s > 0), scaling output channel c of
Conv_i by s_c and dividing input channel c of Conv_{i+1}/Gemm by s_c preserves
the exact float function (S - F == 0 up to FP32 rounding) while equalizing
per-channel weight & activation dynamic ranges for S8/U16 quantization.
"""
import argparse
import hashlib
import os
import sys
import numpy as np
import onnx
from onnx import numpy_helper

CANONICAL_SHA256 = "ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360"


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_compute_chain(model: onnx.ModelProto):
    """Return ordered list of Conv and Gemm nodes in the ColorV2 sequential chain."""
    chain = []
    for node in model.graph.node:
        if node.op_type in ("Conv", "Gemm"):
            chain.append(node)
    return chain


def get_init_map(model: onnx.ModelProto):
    return {init.name: init for init in model.graph.initializer}


def apply_cle(model: onnx.ModelProto, iters: int = 20, eps: float = 1e-6, scale_clip: float = 10.0):
    inits = get_init_map(model)
    chain = find_compute_chain(model)
    if len(chain) < 2:
        raise RuntimeError(f"Expected >= 2 Conv/Gemm layers, found {len(chain)}")

    weights = []
    biases = []
    for node in chain:
        w = numpy_helper.to_array(inits[node.input[1]]).astype(np.float64).copy()
        b = numpy_helper.to_array(inits[node.input[2]]).astype(np.float64).copy() if len(node.input) > 2 else None
        weights.append(w)
        biases.append(b)

    for _ in range(iters):
        for i in range(len(chain) - 1):
            w1 = weights[i]       # [Cout, Cin, kh, kw]
            b1 = biases[i]        # [Cout]
            w2 = weights[i + 1]   # Conv: [Cout2, Cin2, kh, kw] or Gemm: [Cout2, Cin2] (transB=1)
            n2 = chain[i + 1]

            # Determine input-channel axis of w2
            if n2.op_type == "Conv":
                r1 = np.max(np.abs(w1), axis=(1, 2, 3))
                r2 = np.max(np.abs(w2), axis=(0, 2, 3))
            elif n2.op_type == "Gemm":
                trans_b = 0
                for attr in n2.attribute:
                    if attr.name == "transB":
                        trans_b = attr.i
                r1 = np.max(np.abs(w1), axis=(1, 2, 3))
                r2 = np.max(np.abs(w2), axis=0 if trans_b else 1)
            else:
                continue

            if r1.shape != r2.shape:
                raise RuntimeError(f"Channel mismatch between {chain[i].name} ({r1.shape}) and {n2.name} ({r2.shape})")

            # Scale s_c = sqrt(r2 / r1), skip dead channels
            valid = (r1 > eps) & (r2 > eps)
            s = np.ones_like(r1)
            s[valid] = np.sqrt(r2[valid] / r1[valid])
            s = np.clip(s, 1.0 / scale_clip, scale_clip)

            # Apply W1' = S * W1, b1' = S * b1, W2' = W2 * S^-1
            weights[i] = w1 * s[:, None, None, None]
            if b1 is not None:
                biases[i] = b1 * s

            if n2.op_type == "Conv":
                weights[i + 1] = w2 / s[None, :, None, None]
            else:
                trans_b = 0
                for attr in n2.attribute:
                    if attr.name == "transB":
                        trans_b = attr.i
                if trans_b:
                    weights[i + 1] = w2 / s[None, :]
                else:
                    weights[i + 1] = w2 / s[:, None]

    # Write back FP32 initializers
    for i, node in enumerate(chain):
        inits[node.input[1]].CopyFrom(numpy_helper.from_array(weights[i].astype(np.float32), name=node.input[1]))
        if biases[i] is not None:
            inits[node.input[2]].CopyFrom(numpy_helper.from_array(biases[i].astype(np.float32), name=node.input[2]))

    return model


def verify_equivalence(orig_path: str, cle_path: str, n_rand: int = 32, tol: float = 1e-4):
    import onnxruntime as ort

    sess_orig = ort.InferenceSession(orig_path, providers=["CPUExecutionProvider"])
    sess_cle = ort.InferenceSession(cle_path, providers=["CPUExecutionProvider"])
    in_name = sess_orig.get_inputs()[0].name

    rng = np.random.default_rng(20261124)
    xs = rng.uniform(0.0, 1.0, size=(n_rand, 3, 96, 96)).astype(np.float32)

    max_diff = 0.0
    mae_sum = 0.0
    argmax_match = 0
    for i in range(n_rand):
        x = xs[i : i + 1]
        y0 = sess_orig.run(None, {in_name: x})[0]
        y1 = sess_cle.run(None, {in_name: x})[0]
        d = np.abs(y0 - y1)
        max_diff = max(max_diff, float(d.max()))
        mae_sum += float(d.mean())
        if int(np.argmax(y0)) == int(np.argmax(y1)):
            argmax_match += 1

    mae = mae_sum / n_rand
    print(f"[CLE verify] n={n_rand} max_diff={max_diff:.3e} mae={mae:.3e} argmax={argmax_match}/{n_rand}")
    if max_diff >= tol or argmax_match != n_rand:
        raise RuntimeError(f"CLE equivalence gate FAILED: max_diff={max_diff} (tol={tol}), argmax={argmax_match}/{n_rand}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True, help="Canonical v2 color ONNX path")
    ap.add_argument("--out", required=True, help="Output CLE ONNX path")
    ap.add_argument("--iters", type=int, default=20)
    ap.add_argument("--allow-non-canonical", action="store_true")
    args = ap.parse_args()

    h = sha256_file(args.inp)
    print(f"[CLE] input SHA256 = {h}")
    if not args.allow_non_canonical and h != CANONICAL_SHA256:
        print(f"ERROR: input SHA256 mismatch (expected {CANONICAL_SHA256})", file=sys.stderr)
        sys.exit(1)

    model = onnx.load(args.inp)
    apply_cle(model, iters=args.iters)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    onnx.save(model, args.out)
    print(f"[CLE] saved -> {args.out} (SHA256={sha256_file(args.out)})")

    verify_equivalence(args.inp, args.out)
    print("[CLE] PASS: exact float equivalence verified.")


if __name__ == "__main__":
    main()
