#!/bin/bash
set -e

NUMA_NODE=0

NUM_CORES="all"
# NUM_CORES="16"

NUM_PAGES=8

ISRECORD=1  # 1: true, else: false
# ISRECORD=0

RECORDFILE="result1.txt"
# RECORDFILE="56-KLEVV_5600.txt"

benchlist=(
    "BW_ALL_HIT_BASE_SINGLE" 
    "BW_ALL_HIT_BASE_PERCORE" 

    "BW_ALL_HIT_PT_SINGLE" 
    "BW_ALL_HIT_PT_PERCORE"
    # "BW_ALL_HIT_PC_SINGLE"

    # "BW_ALL_HIT_PC_PERCORE"

    "BW_ALL_MISS_BASE_SINGLE"
    "BW_ALL_MISS_BASE_PERCORE"

    "BW_ALL_MISS_PT_SINGLE"
    "BW_ALL_MISS_PT_PERCORE"

    # "BW_ALL_MISS_PC_SINGLE"

    # "BW_ALL_MISS_PC_PERCORE"

    # "BW_ALL_RAND_BASE_SINGLE"

    "BW_ALL_RAND_BASE_PERCORE"
    # "BW_ALL_RAND_PT_SINGLE"
    # "BW_ALL_RAND_PT_PERCORE"

    "BW_ALL_RAND_PC_SINGLE"

    # "BW_ALL_RAND_PC_PERCORE"




    # "BW_1R_HIT_BASE_SINGLE" 
    # "BW_1R_HIT_BASE_PERCORE" 

    # "BW_1R_HIT_PT_SINGLE" 
    # "BW_1R_HIT_PT_PERCORE"
    # "BW_1R_HIT_PC_SINGLE"

    # "BW_1R_HIT_PC_PERCORE"

    # "BW_1R_MISS_BASE_SINGLE"
    # "BW_1R_MISS_BASE_PERCORE"

    # "BW_1R_MISS_PT_SINGLE"
    # "BW_1R_MISS_PT_PERCORE"
    # "BW_1R_MISS_PC_SINGLE"

    # "BW_1R_MISS_PC_PERCORE"

    # "BW_1R_RAND_BASE_SINGLE"

    # "BW_1R_RAND_BASE_PERCORE"
    # "BW_1R_RAND_PT_SINGLE"
    # "BW_1R_RAND_PT_PERCORE"
    # "BW_1R_RAND_PC_SINGLE"
    # "BW_1R_RAND_PC_PERCORE"


    # "TEST"
    # "TEST1"
    # "TEST2"
    # "TEST3"
    # "TEST4"
    # "TEST5"
    # "TEST6"
)

benchstring=$(printf "%s," "${benchlist[@]}")
benchstring=${benchstring%,}

# CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring 2>&1 | tee results.txt"
# CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring 2>&1 | tee results1.txt"
# CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring 2>&1 | tee results2.txt"
CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring"

if [ "$ISRECORD" -eq 1 ]; then
  CMD="$CMD 2>&1 | tee $RECORDFILE"
fi

echo "========================================================"
echo "$CMD"
echo "========================================================"

eval $CMD