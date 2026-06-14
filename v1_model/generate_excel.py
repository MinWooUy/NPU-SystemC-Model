#!/usr/bin/env python3
import os
import sys

# Define all parameter definitions per block
parameters_data = {
    "Systolic Array": [
        {"Parameter": "X", "Type": "int", "Default": "32", "Description": "Columns count (width) of the PE matrix."},
        {"Parameter": "Y", "Type": "int", "Default": "32", "Description": "Rows count (height) of the PE matrix."},
        {"Parameter": "ARITHMETIC", "Type": "int (0/1)", "Default": "1 (FP)", "Description": "Selects integer/fixed-point (0) or floating-point (1) representation."},
        {"Parameter": "MUL_TYPE", "Type": "int", "Default": "0", "Description": "Type of multiplier block used (0=Standard, 1=Approximate)."},
        {"Parameter": "ADD_TYPE", "Type": "int", "Default": "0", "Description": "Type of adder block used (0=Standard, 1=Approximate)."},
        {"Parameter": "M_APPROX", "Type": "float", "Default": "0.0", "Description": "Scaling / configuration parameter for approximate multiplier."},
        {"Parameter": "A_APPROX", "Type": "float", "Default": "0.0", "Description": "Scaling / configuration parameter for approximate adder."},
        {"Parameter": "IA_W", "Type": "int", "Default": "32", "Description": "Activation input bit width."},
        {"Parameter": "IB_W", "Type": "int", "Default": "32", "Description": "Weight input bit width."},
        {"Parameter": "OC_W", "Type": "int", "Default": "32", "Description": "Accumulator / partial sum bit width."},
        {"Parameter": "TH_W", "Type": "int", "Default": "32", "Description": "Zero-gating sparsity threshold register width."},
        {"Parameter": "STAGES_MUL", "Type": "int", "Default": "2", "Description": "Pipeline registers inside the multiplier unit."},
        {"Parameter": "INTERMEDIATE_PIPELINE_STAGE", "Type": "int", "Default": "1", "Description": "Pipeline stage between multiplier and adder blocks."},
        {"Parameter": "ZERO_GATING_MULT", "Type": "bool", "Default": "True", "Description": "Enable zero detection gating at multiplier input."},
        {"Parameter": "ZERO_GATING_ADD", "Type": "bool", "Default": "False", "Description": "Enable zero detection gating at adder input."},
        {"Parameter": "ZD_LOOKAHEAD", "Type": "bool", "Default": "False", "Description": "Lookahead control flag for zero detection."},
        {"Parameter": "EXTRA_CSREG", "Type": "int", "Default": "1", "Description": "Pipeline register level on context switch control lines."}
    ],
    "Ifmap Feeder": [
        {"Parameter": "Y", "Type": "int", "Default": "32", "Description": "Row count matching systolic array height."},
        {"Parameter": "FIFO_POSITIONS", "Type": "int", "Default": "16", "Description": "Queue capacity of each row activation buffer."},
        {"Parameter": "IA_W", "Type": "int", "Default": "32", "Description": "Operand bit width of activations."},
        {"Parameter": "SRAMA_W", "Type": "int", "Default": "1024", "Description": "SRAM A read bus data width (element_width * Y)."},
        {"Parameter": "IDX_W", "Type": "int", "Default": "16", "Description": "Resolution width of index counters."},
        {"Parameter": "ADRA_W", "Type": "int", "Default": "10", "Description": "Address bit width for SRAM A ports."},
        {"Parameter": "DILP_W", "Type": "int", "Default": "64", "Description": "Bit width of dilation pattern registers."},
        {"Parameter": "PARAMS_W", "Type": "int", "Default": "16", "Description": "Control parameter config registers bit width."},
        {"Parameter": "M", "Type": "int", "Default": "1", "Description": "Feeder parallelism replication factor."}
    ],
    "Weight Feeder": [
        {"Parameter": "X", "Type": "int", "Default": "32", "Description": "Column count matching systolic array width."},
        {"Parameter": "FIFO_POSITIONS", "Type": "int", "Default": "16", "Description": "Queue capacity of each column weight buffer."},
        {"Parameter": "IB_W", "Type": "int", "Default": "32", "Description": "Operand bit width of weights."},
        {"Parameter": "SRAMB_W", "Type": "int", "Default": "1024", "Description": "SRAM B read bus data width (element_width * X)."},
        {"Parameter": "IDX_W", "Type": "int", "Default": "16", "Description": "Resolution width of index counters."},
        {"Parameter": "ADRB_W", "Type": "int", "Default": "10", "Description": "Address bit width for SRAM B ports."},
        {"Parameter": "PARAMS_W", "Type": "int", "Default": "16", "Description": "Control parameter config registers bit width."}
    ],
    "PSM Block": [
        {"Parameter": "X", "Type": "int", "Default": "32", "Description": "Columns count matching systolic array width."},
        {"Parameter": "Y", "Type": "int", "Default": "32", "Description": "Rows count matching systolic array height."},
        {"Parameter": "PARAMS_W", "Type": "int", "Default": "16", "Description": "Control configurations parameter width."},
        {"Parameter": "OC_W", "Type": "int", "Default": "32", "Description": "Accumulator / partial sum operand bit width."},
        {"Parameter": "SRAMC_W", "Type": "int", "Default": "1024", "Description": "SRAM C write bus data width (element_width * Y)."},
        {"Parameter": "IDX_W", "Type": "int", "Default": "16", "Description": "Index counter bits resolution."},
        {"Parameter": "ADRC_W", "Type": "int", "Default": "11", "Description": "Address bit width for SRAM C ports."}
    ],
    "Config Registers": [
        {"Parameter": "IF_W", "Type": "int", "Default": "32", "Description": "Data width of host configuration bus."},
        {"Parameter": "IF_ADR_W", "Type": "int", "Default": "32", "Description": "Address width of host configuration bus."},
        {"Parameter": "X", "Type": "int", "Default": "32", "Description": "Active width configuration limits register."},
        {"Parameter": "Y", "Type": "int", "Default": "32", "Description": "Active height configuration limits register."},
        {"Parameter": "TH_W", "Type": "int", "Default": "32", "Description": "Sparsity gating threshold register width."},
        {"Parameter": "ACT_IDX_W", "Type": "int", "Default": "16", "Description": "Activation address index width."},
        {"Parameter": "WEI_IDX_W", "Type": "int", "Default": "16", "Description": "Weight address index width."},
        {"Parameter": "OUT_IDX_W", "Type": "int", "Default": "16", "Description": "Output address index width."},
        {"Parameter": "PARAMS_W", "Type": "int", "Default": "16", "Description": "Internal controls configurations parameters register width."},
        {"Parameter": "DILP_W", "Type": "int", "Default": "64", "Description": "Dilation pattern width register."},
        {"Parameter": "SRAMB_N", "Type": "int", "Default": "8", "Description": "Element parallel slots count in SRAM B bus."}
    ],
    "Main Controller": [
        {"Parameter": "X", "Type": "int", "Default": "32", "Description": "Columns dimension of active grid."},
        {"Parameter": "Y", "Type": "int", "Default": "32", "Description": "Rows dimension of active grid."},
        {"Parameter": "ACT_IDX_W", "Type": "int", "Default": "16", "Description": "Activation index resolution bits."},
        {"Parameter": "OUT_IDX_W", "Type": "int", "Default": "16", "Description": "Output index resolution bits."},
        {"Parameter": "ACT_FIFO_POSITIONS", "Type": "int", "Default": "16", "Description": "Activation feeder queue limit size."},
        {"Parameter": "WEI_FIFO_POSITIONS", "Type": "int", "Default": "16", "Description": "Weight feeder queue limit size."},
        {"Parameter": "PE_LAT", "Type": "int", "Default": "X_DIM + Y_DIM", "Description": "Processing element compute array latency."},
        {"Parameter": "EXTRA_CSREG", "Type": "int", "Default": "1", "Description": "Double context-swapping buffer pipeline level."}
    ]
}

def generate_excel():
    try:
        import pandas as pd
        xlsx_path = "sauria_parameters.xlsx"
        
        # Write multiple sheets to Excel file
        with pd.ExcelWriter(xlsx_path, engine="openpyxl") as writer:
            for sheet_name, rows in parameters_data.items():
                df = pd.DataFrame(rows)
                df.to_excel(writer, sheet_name=sheet_name, index=False)
                
                # Auto-fit columns
                workbook = writer.book
                worksheet = writer.sheets[sheet_name]
                for col in worksheet.columns:
                    max_len = max(len(str(cell.value or '')) for cell in col)
                    col_letter = col[0].column_letter
                    worksheet.column_dimensions[col_letter].width = max(max_len + 3, 12)
                    
        print(f"[SUCCESS] Styled Excel spreadsheet exported successfully: {xlsx_path}")
        return True
    except ImportError:
        print("[WARNING] pandas or openpyxl not installed. Generating CSV files instead.")
        return False

def generate_csvs():
    import csv
    for sheet_name, rows in parameters_data.items():
        filename = f"sauria_parameters_{sheet_name.replace(' ', '_').lower()}.csv"
        with open(filename, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=["Parameter", "Type", "Default", "Description"])
            writer.writeheader()
            for row in rows:
                writer.writerow(row)
        print(f"[SUCCESS] CSV file exported: {filename}")

if __name__ == "__main__":
    if not generate_excel():
        generate_csvs()
