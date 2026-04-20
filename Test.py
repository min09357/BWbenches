#!/usr/bin/env python3
import sys
import subprocess
import os


cpu_base_freq   = "3.7"
dram_bw    = "38.4"
chunk_size = 1
numa_stride = 1

sweep_cores = False # If True, it will run the benchmark multiple times, increasing the number of cores used each time (starting from 1 core up to NUM_CORES). 
verbose_output = True 

extra_args = []

def main():
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} <node_id> [num_cores] [num_pages] <bench_option>")
        print(f"Example: {sys.argv[0]} 0 4 8 BW_ALL,BW_ALL_RM")
        print(f"Example: {sys.argv[0]} 0 ALL 8 BW_ALL,BW_ALL_RM")
        sys.exit(1)

    node_id   = int(sys.argv[1])
    num_cores = sys.argv[2]
    num_pages = sys.argv[3]
    bench     = sys.argv[4]

    subprocess.run(["sudo", "echo", ""], check=True)
    
    subprocess.run(["bash", os.path.expanduser("~/setting.sh")], check=True)

    
    verbose_output = ["-v"]

    if node_id == 0:
        pool_size = 20  # Total number of 1G Hugepages in node
        ch_mask   = "0x0"
        slot_mask = "0x0"
        sch_mask  = "0x82600"
        rank_mask = "0x42120000"
        bg_func   = "0x84042100,0x108404000,0x210808000"
        b_func    = "0x421090000,0x240000"
        col_mask  = "0x1bc0"
        row_mask  = "0x7fff80000"
    # elif node_id == 1:
    else:
        print("Error: Only node 0 is supported.")
        sys.exit(1)

    max_core   = os.cpu_count() - 1
    
    core_list  = []

    if num_cores.lower() == "all":
        print("Use all cores")
        end_core = max_core
    else:
        end_core = node_id + numa_stride * (int(num_cores) - 1)
        if end_core > max_core:
            print(f"Error: Requested cores exceed available cores. Num cores in system: {max_core + 1}")
            sys.exit(1)

    core_list = list(range(node_id, end_core + 1, numa_stride))

    core_list_str = ",".join(str(c) for c in core_list)

    if sweep_cores:
        extra_args.append("-a")
    if verbose_output:
        extra_args.append("-v")

    subprocess.run(["make", "clean"], check=True)
    subprocess.run(["make", "benches", f"-j{os.cpu_count()}"], check=True)

    cmd = [
        "sudo", "numactl",
        "-N", str(node_id),
        "-m", str(node_id),
        "-C", core_list_str,
        "./benches",
        "--num_pages",        str(num_pages),
        "--bench",            bench,
        "--channel_functions", ch_mask,
        "--slot_functions",   slot_mask,
        "--sub_ch_functions", sch_mask,
        "--rank_functions",   rank_mask,
        "--bankgroup_functions", bg_func,
        "--bank_functions",   b_func,
        "--column_bitmask",   col_mask,
        "--row_bitmask",      row_mask,
        "--chunk",            str(chunk_size),
        "--pool_size",        str(pool_size),
        "--cpu_frequency",    cpu_base_freq,
        "--DRAM_bandwidth",   dram_bw,
        "--NUMA_node",        str(node_id),
        "--NUMA_stride",      str(numa_stride),
        *extra_args,
    ]

    print("--------------------------------------------------------")
    print(f"Target Node: {node_id} (Cores: {num_cores})")
    print("Executing Command:")
    print(" ".join(cmd))
    print("--------------------------------------------------------")

    subprocess.run(cmd, check=True)

if __name__ == "__main__":
    main()
