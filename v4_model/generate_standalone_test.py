#!/usr/bin/python3
# Copyright 2026 Barcelona Supercomputing Center (BSC)
# SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
#
# Standalone Sauria core test case generator with full multidimensional configurations

import numpy as np
import os
import argparse

def pack_bits(fields):
    # fields is a list of (value, start_bit, width)
    max_bit = max(start + width for val, start, width in fields)
    num_regs = (max_bit + 31) // 32
    regs = [0] * num_regs
    for val, start, width in fields:
        val = int(val) & ((1 << width) - 1)
        for i in range(width):
            bit_pos = start + i
            reg_idx = bit_pos // 32
            reg_bit = bit_pos % 32
            if (val >> i) & 1:
                regs[reg_idx] |= (1 << reg_bit)
    return regs

def main():
    parser = argparse.ArgumentParser(description="Generate standalone Sauria core test files")
    parser.add_argument('--K', type=int, default=64, help="Tiling/inner loop depth")
    parser.add_argument('--threshold', type=float, default=0.05, help="Zero-gating threshold")
    parser.add_argument('--rows_active', type=str, default="0xFFFFFFFF", help="Hex string of active rows mask (32 bits)")
    parser.add_argument('--cols_active', type=str, default="0xFFFFFFFF", help="Hex string of active cols mask (32 bits)")
    parser.add_argument('--dil_pat', type=int, default=1, help="Dilation pattern")
    parser.add_argument('--seed', type=int, default=42, help="Random seed")
    parser.add_argument('--out_dir', type=str, default="RTL/src/v4_model/tb_data", help="Output directory")

    args = parser.parse_args()

    np.random.seed(args.seed)

    Y_DIM = 32
    X_DIM = 32
    K = args.K
    threshold = args.threshold
    
    # Handle hex or decimal rows_active
    if args.rows_active.startswith("0x") or args.rows_active.startswith("0X"):
        rows_active = int(args.rows_active, 16)
    else:
        rows_active = int(args.rows_active)

    # Handle hex or decimal cols_active
    if args.cols_active.startswith("0x") or args.cols_active.startswith("0X"):
        cols_active = int(args.cols_active, 16)
    else:
        cols_active = int(args.cols_active)
        
    dil_pat = args.dil_pat

    # Make output directory relative to project root
    out_path = args.out_dir
    if not os.path.isabs(out_path):
        proj_root = "/data/XPU00000/users/vuong.nguyen/project/sauria"
        out_path = os.path.join(proj_root, out_path)

    os.makedirs(out_path, exist_ok=True)

    # 1. Generate mat_A (Activations): shape Y_DIM x K
    # Scale between -2.0 and 2.0 to make zero-gating meaningful
    mat_A = np.random.uniform(-2.0, 2.0, (Y_DIM, K))
    # Apply some sparsity (e.g. 25% of elements set to 0.0)
    mask_A = np.random.uniform(0.0, 1.0, (Y_DIM, K)) < 0.25
    mat_A[mask_A] = 0.0

    # 2. Generate mat_B (Weights): shape K x X_DIM
    mat_B = np.random.uniform(-2.0, 2.0, (K, X_DIM))
    mask_B = np.random.uniform(0.0, 1.0, (K, X_DIM)) < 0.2
    mat_B[mask_B] = 0.0

    # Count how many columns are active
    active_cols_count = bin(cols_active).count('1')

    # 3. Compute Golden mat_C: shape Y_DIM x X_DIM
    # With zero-gating threshold emulation
    gold_C = np.zeros((Y_DIM, X_DIM))
    for y in range(Y_DIM):
        # Only compute active rows
        if (rows_active & (1 << y)) != 0:
            for x in range(X_DIM):
                if (cols_active & (1 << x)) != 0:
                    for k in range(K):
                        if abs(mat_A[y, k]) > threshold and abs(mat_B[k, x]) > threshold:
                            gold_C[y, x] += mat_A[y, k] * mat_B[k, x]

    # Pre-skew mat_B in software to align with systolic streaming delay:
    # B is size (K + X_DIM) x X_DIM.
    # For col x, weight at cycle matrix_k is written to B_skewed[matrix_k + x, x]
    mat_B_skewed = np.zeros((K + X_DIM, X_DIM))
    for x in range(X_DIM):
        for k in range(K):
            mat_B_skewed[k + x, x] = mat_B[k, x]

    # 4. Calculate AXI Configuration Register Values
    # Map high-level parameters to SAURIA core registers
    
    # Region 0 (CON):
    # incntlim = K - 1
    # act_reps = 1
    # wei_reps = 1
    # thres: we map threshold to 2-bit thres register. If threshold > 0, set to 1, otherwise 0.
    act_reps = 1
    wei_reps = 1
    thres_reg = 1 if threshold > 0.0 else 0
    
    con_fields = [
        (K - 1, 0, 16),
        (act_reps, 16, 17),
        (wei_reps, 33, 17),
        (thres_reg, 50, 2)
    ]
    con_regs = pack_bits(con_fields)
    
    # Region 1 (ACT):
    # xlim = Y_DIM = 32
    # xstep = SRAMA_N = 32
    # ylim = Y_DIM = 32
    # ystep = Y_DIM = 32
    # chlim = Y_DIM * K = 32 * K = 2048
    # chstep = Y_DIM = 32
    # til_xlim = Y_DIM = 32
    # til_xstep = Y_DIM = 32
    # til_ylim = Y_DIM = 32
    # til_ystep = Y_DIM = 32
    # dil_pat = dil_pat (64-bit)
    # rows_active = rows_active (32-bit)
    act_fields = [
        (Y_DIM, 0, 16),
        (Y_DIM, 16, 16),
        (Y_DIM, 32, 16),
        (Y_DIM, 48, 16),
        (Y_DIM * K, 64, 16),
        (Y_DIM, 80, 16),
        (Y_DIM, 96, 16),
        (Y_DIM, 112, 16),
        (Y_DIM, 128, 16),
        (Y_DIM, 144, 16),
        (dil_pat & 0xFFFFFFFF, 160, 32),
        ((dil_pat >> 32) & 0xFFFFFFFF, 192, 32),
        (rows_active, 224, 32)
    ]
    act_regs = pack_bits(act_fields)
    
    # Region 2 (WEI):
    # waligned = (K % X_DIM == 0) and (active_cols_count == X_DIM)
    waligned = (K % X_DIM == 0) and (active_cols_count == X_DIM)
    waligned_val = 1 if waligned else 0
    # wlim = X_DIM * K
    # wstep = X_DIM
    # auxlim = X_DIM + 1 if not waligned else 1
    # auxstep = X_DIM
    # til_klim = X_DIM
    # til_kstep = X_DIM
    # cols_active = cols_active (32-bit)
    wei_fields = [
        (X_DIM * K, 0, 16),
        (X_DIM, 16, 16),
        (X_DIM + 1 if not waligned else 1, 32, 16),
        (X_DIM, 48, 16),
        (X_DIM, 64, 16),
        (X_DIM, 80, 16),
        (cols_active, 96, 32),
        (waligned_val, 128, 1)
    ]
    wei_regs = pack_bits(wei_fields)
    
    # Region 3 (OUT):
    # ncontexts = 1
    # cxlim = Y_DIM + X_DIM = 64
    # cxstep = X_DIM = 32
    # cklim = Y_DIM * X_DIM = 1024
    # ckstep = Y_DIM = 32
    # til_cylim = Y_DIM = 32
    # til_cystep = Y_DIM = 32
    # til_cklim = Y_DIM * X_DIM = 1024
    # til_ckstep = Y_DIM * X_DIM = 1024
    # inactive_cols = X_DIM - active_cols_count
    # preload_en = 0
    out_fields = [
        (1, 0, 17),
        (Y_DIM + X_DIM, 17, 17),
        (X_DIM, 34, 17),
        (Y_DIM * X_DIM, 51, 17),
        (Y_DIM, 68, 17),
        (Y_DIM, 85, 17),
        (Y_DIM, 102, 17),
        (Y_DIM * X_DIM, 119, 17),
        (Y_DIM * X_DIM, 136, 17),
        (X_DIM - active_cols_count, 153, 8),
        (0, 161, 1)
    ]
    out_regs = pack_bits(out_fields)

    # Save to stand_config.txt
    config_file_path = os.path.join(out_path, "stand_config.txt")
    with open(config_file_path, "w") as f:
        f.write(f"# Standalone Testcase Configuration\n")
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
        f.write(f"act_reps {act_reps}\n")
        f.write(f"wei_reps {wei_reps}\n")
        f.write(f"dil_pat {dil_pat}\n")
        f.write(f"rows_active {rows_active}\n")
        f.write(f"cols_active {cols_active}\n")

    # Save A: Y_DIM x K
    np.savetxt(os.path.join(out_path, "stand_A.txt"), mat_A, fmt='%.6f')
    
    # Save B_skewed: (K + X_DIM) x X_DIM
    np.savetxt(os.path.join(out_path, "stand_B.txt"), mat_B_skewed, fmt='%.6f')

    # Save C: Y_DIM x X_DIM
    np.savetxt(os.path.join(out_path, "stand_C.txt"), gold_C, fmt='%.6f')

    print(f"Standalone testcase generated successfully in {out_path}!")
    print(f"Parameters: K={K}, threshold={threshold}, rows_active={hex(rows_active)}, cols_active={hex(cols_active)}, dil_pat={dil_pat}")

if __name__ == "__main__":
    main()
