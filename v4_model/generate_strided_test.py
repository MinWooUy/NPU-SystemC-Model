#!/usr/bin/python3
# Copyright 2026 Barcelona Supercomputing Center (BSC)
# SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
#
# Standalone Sauria core strided and tiled test case generator

import numpy as np
import os
import argparse

def main():
    parser = argparse.ArgumentParser(description="Generate standalone Sauria core strided test files")
    parser.add_argument('--K', type=int, default=32, help="Effective inner loop depth")
    parser.add_argument('--act_incntstep', type=int, default=2, help="Activation read stride")
    parser.add_argument('--wei_incntstep', type=int, default=2, help="Weight read stride")
    parser.add_argument('--threshold', type=float, default=0.05, help="Zero-gating threshold")
    parser.add_argument('--seed', type=int, default=42, help="Random seed")
    parser.add_argument('--out_dir', type=str, default="RTL/src/v4_model/tb_data_strided", help="Output directory")

    args = parser.parse_args()
    np.random.seed(args.seed)

    Y_DIM = 32
    X_DIM = 32
    K = args.K
    act_incntstep = args.act_incntstep
    wei_incntstep = args.wei_incntstep
    threshold = args.threshold

    out_path = args.out_dir
    if not os.path.isabs(out_path):
        proj_root = "/data/XPU00000/users/vuong.nguyen/project/sauria"
        out_path = os.path.join(proj_root, out_path)

    os.makedirs(out_path, exist_ok=True)

    # Generate full matrices based on the strides
    # Activations A: shape Y_DIM x (K * act_incntstep)
    mat_A_full = np.random.uniform(-2.0, 2.0, (Y_DIM, K * act_incntstep))
    mask_A = np.random.uniform(0.0, 1.0, (Y_DIM, K * act_incntstep)) < 0.25
    mat_A_full[mask_A] = 0.0

    # Weights B: shape (K * wei_incntstep) x X_DIM
    mat_B_full = np.random.uniform(-2.0, 2.0, (K * wei_incntstep, X_DIM))
    mask_B = np.random.uniform(0.0, 1.0, (K * wei_incntstep, X_DIM)) < 0.2
    mat_B_full[mask_B] = 0.0

    # Extract effective matrices fed to NPU
    mat_A_eff = np.zeros((Y_DIM, K))
    for y in range(Y_DIM):
        for k in range(K):
            mat_A_eff[y, k] = mat_A_full[y, k * act_incntstep]

    mat_B_eff = np.zeros((K, X_DIM))
    for x in range(X_DIM):
        for k in range(K):
            mat_B_eff[k, x] = mat_B_full[k * wei_incntstep, x]

    # Compute golden outputs C
    gold_C = np.zeros((Y_DIM, X_DIM))
    for y in range(Y_DIM):
        for x in range(X_DIM):
            for k in range(K):
                if abs(mat_A_eff[y, k]) > threshold and abs(mat_B_eff[k, x]) > threshold:
                    gold_C[y, x] += mat_A_eff[y, k] * mat_B_eff[k, x]

    # Build skewed SRAM B buffer: size (K + X_DIM) * wei_incntstep x X_DIM
    # Place weight mat_B_eff[k, x] at address (k + x) * wei_incntstep
    sramb_size = (K + X_DIM) * wei_incntstep
    mat_B_skewed = np.zeros((sramb_size, X_DIM))
    for x in range(X_DIM):
        for k in range(K):
            addr = (k + x) * wei_incntstep
            mat_B_skewed[addr, x] = mat_B_eff[k, x]

    # Save configuration
    config_file_path = os.path.join(out_path, "stand_config.txt")
    with open(config_file_path, "w") as f:
        f.write(f"# Standalone Strided Testcase Configuration\n")
        f.write(f"K {K}\n")
        f.write(f"threshold {threshold:.6f}\n")
        f.write(f"select 0\n")
        f.write(f"act_incntlim {K * act_incntstep}\n")
        f.write(f"act_incntstep {act_incntstep}\n")
        f.write(f"act_outcntlim {Y_DIM}\n")
        f.write(f"act_outcntstep {Y_DIM}\n")
        f.write(f"wei_incntlim {(K + X_DIM) * wei_incntstep}\n")
        f.write(f"wei_incntstep {wei_incntstep}\n")
        f.write(f"cxlim {Y_DIM + X_DIM}\n")
        f.write(f"cxstep {X_DIM}\n")
        f.write(f"cklim {Y_DIM * X_DIM}\n")
        f.write(f"ckstep {Y_DIM}\n")
        f.write(f"incntlim {K - 1}\n")
        f.write(f"act_reps 1\n")
        f.write(f"wei_reps 1\n")
        f.write(f"dil_pat 1\n")
        f.write(f"rows_active 4294967295\n")
        f.write(f"cols_active 4294967295\n")

    # Save A: Y_DIM x (K * act_incntstep)
    np.savetxt(os.path.join(out_path, "stand_A.txt"), mat_A_full, fmt='%.6f')
    
    # Save B_skewed: sramb_size x X_DIM
    np.savetxt(os.path.join(out_path, "stand_B.txt"), mat_B_skewed, fmt='%.6f')

    # Save C: Y_DIM x X_DIM
    np.savetxt(os.path.join(out_path, "stand_C.txt"), gold_C, fmt='%.6f')

    print(f"Standalone strided testcase generated successfully in {out_path}!")
    print(f"Parameters: K={K}, act_stride={act_incntstep}, wei_stride={wei_incntstep}, threshold={threshold}")

if __name__ == "__main__":
    main()
