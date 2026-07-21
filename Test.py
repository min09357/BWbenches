#!/usr/bin/env python3
import os
import re
import subprocess
import sys

import config
import dram_mapping

# Regex for the benches output line: " - Avg Usage : <value> %"
AVG_USAGE_RE = re.compile(r"-\s*Avg Usage\s*:\s*([\d.]+)\s*%")


def run_benches(cmd, record, record_file):
    """Run the benches command. When `record` is True, stream its output to both
    the console and `record_file` (tee), then return the list of Avg Usage values
    found in order. When False, just run it and return an empty list."""
    if not record:
        subprocess.run(cmd, check=True)
        return []

    os.makedirs(os.path.dirname(record_file) or ".", exist_ok=True)
    usages = []
    with open(record_file, "w") as f:
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
        )
        for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            f.write(line)
            m = AVG_USAGE_RE.search(line)
            if m:
                usages.append(m.group(1))
        ret = proc.wait()
    if ret != 0:
        raise subprocess.CalledProcessError(ret, cmd)
    return usages


def append_usage_csv(record_file, benchstring, usages):
    """Append one comma-joined Avg Usage row to the .csv sibling of record_file,
    writing the benchstring header once when the CSV is first created."""
    if not usages:
        print(f"No 'Avg Usage' values found; skipping CSV update.")
        return
    csv_file = os.path.splitext(record_file)[0] + ".csv"
    new_file = not os.path.exists(csv_file)
    with open(csv_file, "a") as f:
        if new_file:
            f.write(benchstring + "\n")
        f.write(",".join(usages) + "\n")
    print("========================================================")
    print(f"Appended Avg Usage row to {csv_file}:")
    print(",".join(usages))
    print("========================================================")


def main():
    node_id     = int(config.NUMA_NODE)
    num_cores   = str(config.NUM_CORES)
    num_pages   = str(config.NUM_PAGES)
    benchstring = ",".join(config.benchlist)

    if not benchstring:
        print("Error: config.benchlist is empty.")
        sys.exit(1)

    try:
        masks = dram_mapping.PROFILES[config.ADDR_PROFILE]
    except KeyError:
        print(f"Error: unknown ADDR_PROFILE '{config.ADDR_PROFILE}'.")
        print(f"Available profiles: {', '.join(sorted(dram_mapping.PROFILES))}")
        sys.exit(1)

    # Prime sudo so the benches run below doesn't stall on a password prompt.
    subprocess.run(["sudo", "echo", ""], check=True)

    max_core = os.cpu_count() - 1
    if num_cores.lower() == "all":
        print("Use all cores")
        end_core = max_core
    else:
        end_core = node_id + config.numa_stride * (int(num_cores) - 1)
        if end_core > max_core:
            print(f"Error: Requested cores exceed available cores. "
                  f"Num cores in system: {max_core + 1}")
            sys.exit(1)

    core_list = list(range(node_id, end_core + 1, config.numa_stride))
    core_list_str = ",".join(str(c) for c in core_list)

    extra_args = []
    if config.sweep_cores:
        extra_args.append("-a")
    if config.verbose_output:
        extra_args.append("-v")

    subprocess.run(["make", "clean"], check=True)
    subprocess.run(["make", "benches", f"-j{os.cpu_count()}"], check=True)

    cmd = [
        "sudo", "numactl",
        "-N", str(node_id),
        "-m", str(node_id),
        "-C", core_list_str,
        "./benches",
        "--num_pages",           num_pages,
        "--bench",               benchstring,
        "--channel_functions",   masks["ch_func"],
        "--slot_functions",      masks["slot_func"],
        "--sub_ch_functions",    masks["sch_func"],
        "--rank_functions",      masks["rank_func"],
        "--bankgroup_functions", masks["bg_func"],
        "--bank_functions",      masks["ba_func"],
        "--column_bitmask",      masks["col_mask"],
        "--row_bitmask",         masks["row_mask"],
        "--chunk",               str(config.chunk_size),
        "--pool_size",           str(config.pool_size),
        "--cpu_frequency",       str(config.cpu_base_freq),
        "--DRAM_bandwidth",      str(config.dram_bw),
        "--NUMA_node",           str(node_id),
        "--NUMA_stride",         str(config.numa_stride),
        "--num_chains_per_core", str(config.num_chains_per_core),
        "--min_cores",           str(config.min_cores),
        *extra_args,
    ]

    print("--------------------------------------------------------")
    print(f"Target Node: {node_id} (Cores: {num_cores}, Profile: {config.ADDR_PROFILE})")
    print("Executing Command:")
    print(" ".join(cmd))
    print("--------------------------------------------------------")

    record = bool(config.ISRECORD)
    usages = run_benches(cmd, record, config.RECORDFILE)
    if record:
        append_usage_csv(config.RECORDFILE, benchstring, usages)


if __name__ == "__main__":
    main()
