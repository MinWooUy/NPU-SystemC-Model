#!/usr/bin/python3
# Spec-compliant INT8/INT32 Sauria core test case generator

import numpy as np
import os
import argparse

def main():
    parser = argparse.ArgumentParser(description="Generate INT8 NPU spec compliance Sauria core test files")
    parser.add_argument('--K', type=int, default=64, help="Tiling/inner loop depth")
    parser.add_argument('--threshold', type=float, default=0.00, help="Zero-gating threshold (0 for exact INT8)")
    parser.add_argument('--seed', type=int, default=123, help="Random seed")
    parser.add_argument('--out_dir', type=str, default="RTL/src/v4_model/tb_data_spec", help="Output directory")

    args = parser.parse_args()
    np.random.seed(args.seed)

    Y_DIM = 32
    X_DIM = 32
    K = args.K
    threshold = args.threshold
    rows_active = 0xFFFFFFFF
    cols_active = 0xFFFFFFFF
    dil_pat = 1

    # Make output directory relative to project root
    out_path = args.out_dir
    if not os.path.isabs(out_path):
        proj_root = "/data/XPU00000/users/vuong.nguyen/project/sauria"
        out_path = os.path.join(proj_root, out_path)

    os.makedirs(out_path, exist_ok=True)

    # 1. Generate mat_A (Activations - Asymmetric INT8): shape Y_DIM x K
    # Values between 0 and 127 to simulate asymmetric activation range
    mat_A = np.random.randint(0, 128, (Y_DIM, K)).astype(np.float32)

    # 2. Generate mat_B (Weights - Symmetric INT8): shape K x X_DIM
    # Values between -127 and 127 to simulate symmetric weights
    mat_B = np.random.randint(-127, 128, (K, X_DIM)).astype(np.float32)

    # 3. Compute Golden mat_C (INT32 accumulator results): shape Y_DIM x X_DIM
    gold_C = np.dot(mat_A, mat_B)

    # Pre-skew mat_B in software to align with systolic streaming delay:
    mat_B_skewed = np.zeros((K + X_DIM, X_DIM))
    for x in range(X_DIM):
        for k in range(K):
            mat_B_skewed[k + x, x] = mat_B[k, x]

    # Save configuration
    config_file_path = os.path.join(out_path, "spec_config.txt")
    with open(config_file_path, "w") as f:
        f.write(f"# NPU Specification Testcase Configuration\n")
        f.write(f"K {K}\n")
        f.write(f"threshold {threshold:.6f}\n")
        f.write(f"select 0\n")
        f.write(f"act_incntlim {Y_DIM}\n")
        f.write(f"act_incntstep {Y_DIM}\n")
        f.write(f"act_outcntlim {Y_DIM}\n")
        f.write(f"act_outcntstep {Y_DIM}\n")
        f.write(f"wei_incntlim {X_DIM * K}\n")
        f.write(f"wei_incntstep {X_DIM}\n")
        f.write(f"cxlim {Y_DIM + X_DIM}\n")
        f.write(f"cxstep {X_DIM}\n")
        f.write(f"cklim {Y_DIM * X_DIM}\n")
        f.write(f"ckstep {Y_DIM}\n")
        f.write(f"incntlim {K - 1}\n")
        f.write(f"act_reps 1\n")
        f.write(f"wei_reps 1\n")
        f.write(f"dil_pat {dil_pat}\n")
        f.write(f"rows_active {rows_active}\n")
        f.write(f"cols_active {cols_active}\n")

    # Save as integers to files
    np.savetxt(os.path.join(out_path, "spec_A.txt"), mat_A, fmt='%d')
    np.savetxt(os.path.join(out_path, "spec_B.txt"), mat_B_skewed, fmt='%d')
    np.savetxt(os.path.join(out_path, "spec_C.txt"), gold_C, fmt='%d')

    print(f"INT8 spec-compliant testcase generated successfully in {out_path}!")
    print(f"Parameters: K={K}, threshold={threshold}")

if __name__ == "__main__":
    main()
