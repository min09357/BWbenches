#include "utils.h"



vector<uint64_t> parseHexList(const string& input) {
    vector<uint64_t> list;
    stringstream ss(input);
    string segment;
    while (getline(ss, segment, ',')) {
        list.push_back(strtoull(segment.c_str(), nullptr, 16));
    }
    return list;
}

vector<string> parseBenchList(const string& input) {
    vector<string> list;
    stringstream ss(input);
    string segment;
    while (getline(ss, segment, ',')) {
        list.push_back(segment);
    }
    return list;
}

void print_help(const char* prog_name) {
    // todo: update
    cout << "Usage: " << prog_name << " [options]\n"
         << "Options:\n"
         << "  -B, --bench <list>             Comma-separated list (e.g., BWSB,DG,BW_ALL)\n"
         << "  -p, --num_pages <num>          Number of 1GB hugepages to use\n"
         << "  -P, --pool_size <num>          Total pool size of hugepages to allocate for filtering\n"
         << "  -c, --channel_functions <list> CSV hex masks (e.g. 0x1,0x2) (def: 0x0)\n"
         << "  -s, --slot_functions <list>    CSV hex masks (def: 0x0)\n"
         << "  -S, --sub_ch_functions <list>  CSV hex masks (def: 0x0)\n"
         << "  -r, --rank_functions <list>    CSV hex masks (def: 0x0)\n"
         << "  -g, --bankgroup_functions <list> CSV hex masks (required for DG)\n"
         << "  -b, --bank_functions <list>    CSV hex masks (required for SBDR)\n"
         << "  -R, --row_bitmask <mask>       Single hex mask for row bits (e.g. 0x3fff1c000)\n"
         << "  -C, --column_bitmask <mask>    Single hex mask for col bits (e.g. 0x3f80)\n"
         << "  -e, --seed <seed>              Random seed (optional)\n"
         << "  -k, --chunk <size>             OpenMP schedule chunk size (default: 1)\n"
         << "  -d, --debug                    Enable debug output in measureBandwidth\n"
         << "  -a, --all                      Test with varying core counts (1,2,4,8,16...)\n"
         << "  -v, --verbose                  verbose output\n";
}

void print_result(BenchResult res) {
    cout << fixed << setprecision(3);
    cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
    cout << " - Avg Usage : " << res.usage << " %" << endl;
    cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
    cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
    cout << " - Elements     : " << setprecision(3) << res.valid_count << " (" << res.valid_count*64/10e9 << " GB)" << endl;
    cout << "========================================================" << endl;
}


uint64_t getPhysicalAddr(void* vaddr) {
    static thread_local int fd = -1;
    if (fd < 0) {
        fd = open("/proc/self/pagemap", O_RDONLY);
        if (fd < 0) return 0;
    }

    uint64_t v = reinterpret_cast<uint64_t>(vaddr);
    uint64_t offset = (v / BASE_PAGE_SIZE) * sizeof(uint64_t);

    uint64_t entry = 0;
    ssize_t bytes_read = pread(fd, &entry, sizeof(entry), offset);

    if (bytes_read != sizeof(entry)) return 0;

    uint64_t pfn = entry & ((((uint64_t)1 << 55) - 1));
    if (pfn == 0) return 0;

    uint64_t offset_in_page = v % BASE_PAGE_SIZE;
    return (pfn * BASE_PAGE_SIZE) + offset_in_page;
}

int applyXorFunctions(uint64_t physAddr, const vector<uint64_t>& masks) {
    int result = 0;
    for (size_t i = 0; i < masks.size(); ++i) {
        if (__builtin_parityl(physAddr & masks[i])) {
            result |= (1 << i);
        }
    }
    return result;
}

BankInfo getBankFromPhysicalAddr(uint64_t physAddr) {
    BankInfo info;
    info.ch   = applyXorFunctions(physAddr, g_config.ch_masks);
    info.slot = applyXorFunctions(physAddr, g_config.slot_masks);
    info.sc   = applyXorFunctions(physAddr, g_config.sub_ch_masks);
    info.rk   = applyXorFunctions(physAddr, g_config.rank_masks);
    info.bg   = applyXorFunctions(physAddr, g_config.bg_masks);
    info.bank = applyXorFunctions(physAddr, g_config.bank_masks);
    info.row  = (int)_pext_u64(physAddr, g_config.row_mask);
    info.col  = (int)_pext_u64(physAddr, g_config.col_mask);
    return info;
}

int calcNum(const vector<uint64_t>& masks) {
    if (masks.size() == 1 && masks[0] == 0) return 1;
    return 1 << masks.size();
}

int countSetBits(uint64_t n) {
    int count = 0;
    while (n > 0) {
        n &= (n - 1);
        count++;
    }
    return count;
}

int getMaxBit() {
    uint64_t all_masks = 0;
    auto accumulate_masks = [&](const vector<uint64_t>& masks) {
        for(uint64_t m : masks) all_masks |= m;
    };

    accumulate_masks(g_config.ch_masks);
    accumulate_masks(g_config.slot_masks);
    accumulate_masks(g_config.sub_ch_masks);
    accumulate_masks(g_config.rank_masks);
    accumulate_masks(g_config.bg_masks);
    accumulate_masks(g_config.bank_masks);

    all_masks |= g_config.col_mask;
    all_masks |= g_config.row_mask;

    if (all_masks == 0) return 0;
    return 63 - __builtin_clzll(all_masks);
}

// Detect bank function bits, not duplicated with row, col bits.
int getMaxOnlyFuncBit() {
    uint64_t functionmask = ~(g_config.col_mask | g_config.row_mask);

    uint64_t all_masks = 0;
    auto accumulate_masks = [&](const vector<uint64_t>& masks) {
        for(uint64_t m : masks) all_masks |= m;
    };

    accumulate_masks(g_config.ch_masks);
    accumulate_masks(g_config.slot_masks);
    accumulate_masks(g_config.sub_ch_masks);
    accumulate_masks(g_config.rank_masks);
    accumulate_masks(g_config.bg_masks);
    accumulate_masks(g_config.bank_masks);

    uint64_t onlyfunctionmask = all_masks & functionmask;

    if (onlyfunctionmask == 0) return 0;
    return 63 - __builtin_clzll(onlyfunctionmask);
}


vector<int> get_thread_list() {
    vector<int> thread_list;
    int max_hardware_threads = omp_get_max_threads();
    if (g_config.test_all_cores) {
        int start_val = g_config.min_cores < max_hardware_threads ? g_config.min_cores : max_hardware_threads;
        for (int t = start_val; t <= max_hardware_threads; t++) {
            thread_list.push_back(t);
        }
    } else {
        thread_list.push_back(max_hardware_threads);
    }
    return thread_list;
}