#!/bin/bash
set -e

benchlist=(
    "BW_ALL_HIT_BASE_SINGLE" 
    "BW_ALL_HIT_BASE_PERCORE" 
    "BW_ALL_HIT_PT_SINGLE" 
    "BW_ALL_HIT_PT_PERCORE"
    "BW_ALL_HIT_PC_SINGLE"
    "BW_ALL_HIT_PC_PERCORE"

    "BW_ALL_MISS_BASE_SINGLE"
    "BW_ALL_MISS_BASE_PERCORE"
    "BW_ALL_MISS_PT_SINGLE"
    "BW_ALL_MISS_PT_PERCORE"
    "BW_ALL_MISS_PC_SINGLE"
    "BW_ALL_MISS_PC_PERCORE"

    "BW_ALL_RAND_BASE_SINGLE"
    "BW_ALL_RAND_BASE_PERCORE"
    "BW_ALL_RAND_PT_SINGLE"
    "BW_ALL_RAND_PT_PERCORE"
    "BW_ALL_RAND_PC_SINGLE"
    "BW_ALL_RAND_PC_PERCORE"


    "BW_1R_HIT_BASE_SINGLE" 
    "BW_1R_HIT_BASE_PERCORE" 
    "BW_1R_HIT_PT_SINGLE" 
    "BW_1R_HIT_PT_PERCORE"
    "BW_1R_HIT_PC_SINGLE"
    "BW_1R_HIT_PC_PERCORE"

    "BW_1R_MISS_BASE_SINGLE"
    "BW_1R_MISS_BASE_PERCORE"
    "BW_1R_MISS_PT_SINGLE"
    "BW_1R_MISS_PT_PERCORE"
    "BW_1R_MISS_PC_SINGLE"
    "BW_1R_MISS_PC_PERCORE"

    "BW_1R_RAND_BASE_SINGLE"
    "BW_1R_RAND_BASE_PERCORE"
    "BW_1R_RAND_PT_SINGLE"
    "BW_1R_RAND_PT_PERCORE"
    "BW_1R_RAND_PC_SINGLE"
    "BW_1R_RAND_PC_PERCORE"
)

benchstring=$(printf "%s," "${benchlist[@]}")
benchstring=${benchstring%,}

CMD="python3 ./Test.py 0 all 8 $benchstring 2>&1 | tee results.txt"

echo "========================================================"
echo "$CMD"
echo "========================================================"

eval $CMD