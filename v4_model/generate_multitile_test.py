#!/usr/bin/python3
# Copyright 2026 Barcelona Supercomputing Center (BSC)
# SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
#
# Standalone Sauria core multi-tile, strided, and dilated test case generator

import numpy as np
import os
import argparse

def main():
    parser = argparse.ArgumentParser(description="Generate standalone Sauria core multi-tile test files")
    parser.add_argument('--K', type=int, default=24, help="Effective inner loop depth")
    parser.add_argument('--act_incntstep', type=int, default=3, help="Activation read stride")
    parser.add_argument('--wei_incntstep', type=int, default=3, help="Weight read stride")
    parser.add_argument('--threshold', type=float, default=0.05, help="Zero-gating threshold")
    parser.add_argument('--seed', type=int, default=100, help="Random seed")
    parser.add_argument('--out_dir', type=str, default="RTL/src/v4_model/tb_data_multitile", help="Output directory")

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

    # We generate two tiles: Tile 0 and Tile 1
    for t in range(2):
        # Generate full matrices based on the strides
        mat_A_full = np.random.uniform(-2.0, 2.0, (Y_DIM, K * act_incntstep))
        mask_A = np.random.uniform(0.0, 1.0, (Y_DIM, K * act_incntstep)) < 0.2
        mat_A_full[mask_A] = 0.0

        mat_B_full = np.random.uniform(-2.0, 2.0, (K * wei_incntstep, X_DIM))
        mask_B = np.random.uniform(0.0, 1.0, (K * wei_incntstep, X_DIM)) < 0.15
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

        # Build skewed SRAM B buffer
        sramb_size = (K + X_DIM) * wei_incntstep
        mat_B_skewed = np.zeros((sramb_size, X_DIM))
        for x in range(X_DIM):
            for k in range(K):
                addr = (k + x) * wei_incntstep
                mat_B_skewed[addr, x] = mat_B_eff[k, x]

        # Save files for each tile
        np.savetxt(os.path.join(out_path, f"stand_A{t}.txt"), mat_A_full, fmt='%.6f')
        np.savetxt(os.path.join(out_path, f"stand_B{t}.txt"), mat_B_skewed, fmt='%.6f')
        np.savetxt(os.path.join(out_path, f"stand_C{t}.txt"), gold_C, fmt='%.6f')

    # Save configuration
    config_file_path = os.path.join(out_path, "stand_config.txt")
    with open(config_file_path, "w") as f:
        f.write(f"# Standalone Multi-Tile Testcase Configuration\n")
        f.write(f"K {K}\n")
        f.write(f"threshold {threshold:.6f}\n")
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
        f.write(f"act_reps 2\n")
        f.write(f"wei_reps 2\n")
        f.write(f"dil_pat 3\n")
        f.write(f"rows_active 4294967295\n")

    print(f"Multi-tile testcase generated successfully in {out_path}!")
    print(f"Parameters: K={K}, act_stride={act_incntstep}, wei_stride={wei_incntstep}, threshold={threshold}, dil_pat=3")

if __name__ == "__main__":
    main()
