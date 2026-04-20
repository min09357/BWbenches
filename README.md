# Bwbenches

## Environment setup

* alloc hugepages
* fix core/uncore frequency (to base cpu freq. with intel pepc)
* disable turbo boost (intel pepc)
* disable prefetch (sudo wrmsr -a 0x1a4 0x2f) 

## Edit ./Test.py

```
# global
cpu_base_freq,
dram_bw,
numa_stride,

## optional
sweep_cores,
min_cores,
num_chains_per_core,


# main()
pool_size,
ch_func,
slot_func,
sch_func,
rank_func,
bg_func,
ba_func,
col_mask,
row_mask,

```


## Run

```bash
./Test.sh
```

기본적으로 코어 개수를 3개~max까지 전부 sweep해가며 측정하는데,

출력을 보고 일찍 saturate된다면 ./Test.sh에서 NUM_CORES의 값을 all에서 16 등으로 바꾸면 됩니다.

반대로 너무 늦게 saturate된다면 ./Test.py에서 min_cores의 값을 늘려주면 됩니다.

## Bench options

```
BW\_{rank option}\_{access pattern}\_{access method}\_{access sequences}
ex) BW_ALL_HIT_PT_SINGLE
```

### rank option

```
ALL: all ranks
1R: 1 rank
```

### access pattern

```
HIT: Row hit with interleaving
MISS: Row miss with interleaving
RAND: Random access
```

### access method

```
BASE: Fetch next access address from DRAM(LLC), in every access. About 12.5% additional DRAM access.
PT: Use patterns to decode address. Almost zero overhead.
PC: Pointer Chasing. No overhead but should modify num chains per cores.
```

### access sequence

```
SINGLE: Use one, huge address sequence. Allocated to each core with no overlap.
PERCORES: Use small address sequence for each core.
```
