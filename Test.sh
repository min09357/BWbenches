#!/bin/bash
set -e

if [ "$#" -eq 4 ]; then
    NODE_ID=$1
    NUM_CORES=$2
    NUM_PAGES=$3
    BENCH=$4
else
    echo "Usage: $0 <node_id> [num_cores] [num_pages] <bench_option>"
    echo "Example: $0 0 4 8 BW_ALL,BW_ALL_RM"
    echo "Example: $0 0 ALL 8 BW_ALL,BW_ALL_RM"
    exit 1
fi

sudo echo ""

~/setting.sh

# cpu의 base frequency (__rdtsc()로 측정한 사이클을 시간으로 변환하기 위한 값. CPU의 frequency롤 다른 값으로 고정해도 TSC의 주파수는 변하지 않음)
CPU_FREQ=3.7

CHUNK_SIZE=1
EXTRA_ARGS=""

DRAM_BW="44.8"
NUMA_STRIDE="1"



# CORE_TEST="-a"    # 측정에 사용하는 코어의 개수를 1개부터 NUM_CORES까지 늘려가며 모든 경우에 대해 측정.

VERBOSE_OUTPUT="-v"

if [ "$NODE_ID" -eq 0 ]; then

    POOL_SIZE=20

    SCH_MASK="0x82600"

    BG_FUNC="0x84042100,0x108404000,0x210808000"
    B_FUNC="0x421090000,0x240000"


    COL_MASK="0x1bc0"
    ROW_MASK="0x7fff80000"

    RANK_OP="--rank_functions 0x42120000"
    DRAM_BW="38.4"

    START_CORE=0

else
    echo "Error: Only node 0 and 1 are supported."
    exit 1
fi



# sudo sh -c "echo 0 > /sys/devices/system/node/node$NUM_CORES/hugepages/hugepages-1048576kB/nr_hugepages"
# sudo sh -c "echo $NUM_PAGES > /sys/devices/system/node/node$NUM_CORES/hugepages/hugepages-1048576kB/nr_hugepages"



# 3. 코어 리스트 생성
CORE_LIST=""
MAX_CORE=$(($(nproc --all) - 1)) # 시스템의 전체 코어 수 확인

CORE_STRIDE="1"

if [ "$NUM_CORES" = "all" ]; then
    echo "Use all cores"
    for (( i=$START_CORE; i<=$MAX_CORE; i+=$CORE_STRIDE )); do
        if [ -z "$CORE_LIST" ]; then CORE_LIST="$i"; else CORE_LIST="$CORE_LIST,$i"; fi
    done
else
    # 지정된 개수만큼 사용
    COUNT=0
    CURRENT_CORE=$START_CORE
    while [ "$COUNT" -lt "$NUM_CORES" ]; do
        if [ -z "$CORE_LIST" ]; then CORE_LIST="$CURRENT_CORE"; else CORE_LIST="$CORE_LIST,$CURRENT_CORE"; fi
        CURRENT_CORE=$((CURRENT_CORE + $CORE_STRIDE))
        if [ "$CURRENT_CORE" -gt "$MAX_CORE" ]; then break; fi # 시스템 범위를 넘으면 중단
        COUNT=$((COUNT + 1))
    done
fi

# 4. 컴파일
make clean
make benches -j$(nproc)

# 5. 실행 명령어 구성 및 출력

CMD="sudo numactl -N $NODE_ID -m $NODE_ID -C $CORE_LIST ./benches \
    --num_pages $NUM_PAGES \
    --bench $BENCH \
    --sub_ch_functions $SCH_MASK \
    $RANK_OP\
    --bankgroup_functions $BG_FUNC \
    --bank_functions $B_FUNC \
    --column_bitmask $COL_MASK \
    --row_bitmask $ROW_MASK \
    --chunk $CHUNK_SIZE \
    --pool_size $POOL_SIZE \
    --cpu_frequency $CPU_FREQ \
    --DRAM_bandwidth $DRAM_BW \
    --NUMA_node $NODE_ID \
    --NUMA_stride $NUMA_STRIDE \
    $CORE_TEST\
    $VERBOSE_OUTPUT\
    $EXTRA_ARGS"


echo "--------------------------------------------------------"
echo "Target Node: $NODE_ID (Cores: $NUM_CORES)"
echo "Executing Command:"
echo "$CMD"
echo "--------------------------------------------------------"

eval $CMD

