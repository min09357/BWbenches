#!/bin/bash
set -e

NUMA_NODE=0

NUM_CORES="all"
# NUM_CORES="16"

# NUM_PAGES=8
NUM_PAGES=16

ISRECORD=1  # 1: true, else: false
# ISRECORD=0

RECORDFILE="results/result.txt"
# RECORDFILE="56-KLEVV_5600.txt"

benchlist=(

  "BW_R0_HIT_PT_SINGLE" 
  # "BW_R0_HIT_PT_PERCORE"
  "BW_R0_MISS_PT_SINGLE"
  # "BW_R0_MISS_PT_PERCORE"
  "BW_R0_RAND_PC_SINGLE"

  "BW_ALL_HIT_PT_SINGLE" 
  # "BW_ALL_HIT_PT_PERCORE"
  "BW_ALL_MISS_PT_SINGLE"
  # "BW_ALL_MISS_PT_PERCORE"
  "BW_ALL_RAND_PC_SINGLE"






  # "BW_R0_SC0_HIT_PT_SINGLE" 
  # "BW_R0_SC0_HIT_PT_PERCORE"
  # "BW_R0_SC0_MISS_PT_SINGLE"
  # "BW_R0_SC0_MISS_PT_PERCORE"
  # "BW_R0_SC0_RAND_PC_SINGLE"

  # "BW_ALL_SC0_HIT_PT_SINGLE" 
  # "BW_ALL_SC0_HIT_PT_PERCORE"
  # "BW_ALL_SC0_MISS_PT_SINGLE"
  # "BW_ALL_SC0_MISS_PT_PERCORE"
  # "BW_ALL_SC0_RAND_PC_SINGLE"

  # "BW_ALL_SC1_HIT_PT_SINGLE" 
  # "BW_ALL_SC1_HIT_PT_PERCORE"
  # "BW_ALL_SC1_MISS_PT_SINGLE"
  # "BW_ALL_SC1_MISS_PT_PERCORE"
  # "BW_ALL_SC1_RAND_PC_SINGLE"






  # "BW_R1_HIT_PT_SINGLE" 
  # "BW_R1_HIT_PT_PERCORE"
  # "BW_R1_MISS_PT_SINGLE"
  # "BW_R1_MISS_PT_PERCORE"
  # "BW_R1_RAND_PC_SINGLE"


  # "BW_ALL_BA2_HIT_PT_SINGLE" 
  # # "BW_ALL_BA2_HIT_PT_PERCORE"
  # "BW_ALL_BA2_MISS_PT_SINGLE"
  # # "BW_ALL_BA2_MISS_PT_PERCORE"
  # "BW_ALL_BA2_RAND_PC_SINGLE"

  # "BW_R0_BA2_HIT_PT_SINGLE" 
  # # "BW_R0_BA2_HIT_PT_PERCORE"
  # "BW_R0_BA2_MISS_PT_SINGLE"
  # # "BW_R0_BA2_MISS_PT_PERCORE"
  # "BW_R0_BA2_RAND_PC_SINGLE"





  # "BW_ALL_BA3_HIT_PT_SINGLE" 
  # # "BW_ALL_BA3_HIT_PT_PERCORE"
  # "BW_ALL_BA3_MISS_PT_SINGLE"
  # # "BW_ALL_BA3_MISS_PT_PERCORE"
  # "BW_ALL_BA3_RAND_PC_SINGLE"

  # "BW_R0_BA3_HIT_PT_SINGLE" 
  # # "BW_R0_BA3_HIT_PT_PERCORE"
  # "BW_R0_BA3_MISS_PT_SINGLE"
  # # "BW_R0_BA3_MISS_PT_PERCORE"
  # "BW_R0_BA3_RAND_PC_SINGLE"

  

  




)

benchstring=$(printf "%s," "${benchlist[@]}")
benchstring=${benchstring%,}

# CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring 2>&1 | tee results.txt"
# CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring 2>&1 | tee results1.txt"
# CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring 2>&1 | tee results2.txt"
CMD="python3 ./Test.py $NUMA_NODE $NUM_CORES $NUM_PAGES $benchstring"

if [ "$ISRECORD" -eq 1 ]; then
  mkdir -p "$(dirname "$RECORDFILE")"
  CMD="$CMD 2>&1 | tee $RECORDFILE"
fi

echo "========================================================"
echo "$CMD"
echo "========================================================"

eval $CMD

# --- After result.txt is fully written: extract Avg Usage into a CSV row ---
if [ "$ISRECORD" -eq 1 ]; then
  CSVFILE="${RECORDFILE%.txt}.csv"

  # Extract each " - Avg Usage : <value> %" in order, joined by commas
  usage_row=$(awk '/- Avg Usage/{printf "%s%s", (n++?",":""), $(NF-1)} END{print ""}' "$RECORDFILE")

  if [ -n "$usage_row" ]; then
    # Write the benchmark-name header once, only when the CSV is first created
    if [ ! -f "$CSVFILE" ]; then
      echo "$benchstring" > "$CSVFILE"
    fi
    echo "$usage_row" >> "$CSVFILE"

    echo "========================================================"
    echo "Appended Avg Usage row to $CSVFILE:"
    echo "$usage_row"
    echo "========================================================"
  else
    echo "No 'Avg Usage' values found in $RECORDFILE; skipping CSV update."
  fi
fi