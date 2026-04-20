#include "types.h"
#include "utils.h"
#include "classification.h"
#include "stream.h"
#include "bandwidth.h"



int main(int argc, char* argv[])
{
    // 1. Argument Parsing
    static struct option long_options[] = {
        {"bench",               required_argument, 0, 'B'},
        {"num_pages",           required_argument, 0, 'p'},
        {"pool_size",           required_argument, 0, 'P'},
        {"channel_functions",   required_argument, 0, 'c'},
        {"slot_functions",      required_argument, 0, 's'},
        {"sub_ch_functions",    required_argument, 0, 'S'},
        {"rank_functions",      required_argument, 0, 'r'},
        {"bankgroup_functions", required_argument, 0, 'g'},
        {"bank_functions",      required_argument, 0, 'b'},
        {"row_bitmask",         required_argument, 0, 'R'},
        {"column_bitmask",      required_argument, 0, 'C'},
        {"DRAM_bandwidth",      required_argument, 0, 'W'},
        {"seed",                required_argument, 0, 'e'},
        {"chunk",               required_argument, 0, 'k'},
        {"cpu_frequency",       required_argument, 0, 'f'},
        {"NUMA_node",           required_argument, 0, 'N'},
        {"NUMA_stride",         required_argument, 0, 'D'},
        {"all",                 no_argument,       0, 'a'},
        {"verbose",             no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "B:p:P:c:s:S:r:g:b:R:C:W:e:k:f:Nav", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'B': g_config.bench = optarg; break;
            case 'p': g_config.numHugePages = stoi(optarg); break;
            case 'P': g_config.poolSize = stoi(optarg); break;
            case 'c': g_config.ch_masks = parseHexList(optarg); break;
            case 's': g_config.slot_masks = parseHexList(optarg); break;
            case 'S': g_config.sub_ch_masks = parseHexList(optarg); break;
            case 'r': g_config.rank_masks = parseHexList(optarg); break;
            case 'g': g_config.bg_masks = parseHexList(optarg); break;
            case 'b': g_config.bank_masks = parseHexList(optarg); break;
            case 'R': g_config.row_mask = strtoull(optarg, nullptr, 16); break;
            case 'C': g_config.col_mask = strtoull(optarg, nullptr, 16); break;
            case 'W': g_config.DRAM_BW = stof(optarg); break;
            case 'e': g_config.seed = stoul(optarg); g_config.seed_provided = true; break;
            case 'a': g_config.test_all_cores = true; break;
            case 'v': g_config.verbose_output = true; break;
            case 'k': g_config.chunk_size = stoi(optarg); break;
            case 'f': g_config.cpu_period = 1.0 / stof(optarg); break;
            case 'N': g_config.NUMA_node = stoi(optarg); break;
            case 'D': g_config.NUMA_stride = stoi(optarg); break;
            default: print_help(argv[0]); return -1;
        }
    }

    if (g_config.bench.empty()) {
        cerr << "[ERROR] --bench is required.\n";
        print_help(argv[0]);
        return -1;
    }

    // Parse benchmark list (comma separated)
    vector<string> target_benchmarks = parseBenchList(g_config.bench);
    static const set<string> IMPLEMENTED_BENCHMARKS = {
        "BW_ALL_RD","BW_ALL", "BW_ALL_OLD", "BW_ALL_RM", "BW_ALL_RM_OLD", "BW_ALL_WO_RK", "BW_ALL_WO_RK_RM",
        "BW_SB", "BW_SB_RM", "BW_DG", "BW_DG_4", "BW_DOUBLE_BANK", "BW_HALF_BANK", "BW_HALF_BANK_WO_RK", "BW_HALF_BANK_RM",
        "BW_SINGLE_BA", "BW_SINGLE_BA_RM", "BW_SINGLE_BA_WO_RK_RM", "BW_SINGLE_BA_OLD", "BW_SINGLE_BA_RM_OLD", "BW_SINGLE_BA_RD",
        "BW_DOUBLE_BA", "BW_DOUBLE_BA_RM", "BW_DOUBLE_BA_WO_RK_RM", "BW_DOUBLE_BA_OLD", "BW_DOUBLE_BA_RM_OLD", "BW_DOUBLE_BA_RD",
        "TEST", "TEST1", "TEST2", "TEST3", "TEST4", "TEST5", "TEST6"
    };
    for (const string& b : target_benchmarks) {
        if (IMPLEMENTED_BENCHMARKS.find(b) == IMPLEMENTED_BENCHMARKS.end()) {
            cerr << "[ERROR] Unknown benchmark type: " << b << endl;
            return -1;
        }
    }

    if (g_config.ch_masks.empty()) g_config.ch_masks.push_back(0);
    if (g_config.slot_masks.empty()) g_config.slot_masks.push_back(0);
    if (g_config.sub_ch_masks.empty()) g_config.sub_ch_masks.push_back(0);
    if (g_config.rank_masks.empty()) g_config.rank_masks.push_back(0);
    if (g_config.bg_masks.empty()) g_config.bg_masks.push_back(0);
    if (g_config.bank_masks.empty()) g_config.bank_masks.push_back(0);

    if (g_config.numHugePages <= 0) {
        cerr << "[ERROR] --num_pages is required.\n";
        print_help(argv[0]);
        return -1;
    }

    // Pool size default if not set
    if (g_config.poolSize < g_config.numHugePages) {
        g_config.poolSize = g_config.numHugePages;
        cout << "PoolSize was not provided, so it was automatically set to max to numHugePages" << endl;
    }

    // 2. Initialization & Allocation


    vector<pair<void*,uint64_t>> poolPages(g_config.poolSize);

    int allocated_pages = 0;
    for (int i = 0; i < g_config.poolSize; i++) {
        void* addr = mmap(nullptr, HUGEPAGE_SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
        if (addr == MAP_FAILED) {
            cerr << "[ERROR] mmap() failed for pool page " << i << ". Errno: " << strerror(errno) << endl;
            break;
        }
        volatile char* ptr = reinterpret_cast<volatile char*>(addr);
        ptr[0] = 1;
        uint64_t paddr = getPhysicalAddr(addr);
        poolPages[i] = make_pair(addr, paddr);
        allocated_pages++;
    }
    poolPages.resize(allocated_pages);

    if (allocated_pages < g_config.numHugePages) {
        cerr << "[ERROR] Only allocated " << allocated_pages << " of " << g_config.numHugePages << " requested pages." << endl;
        for (auto& p : poolPages) munmap(p.first, HUGEPAGE_SIZE);
        return -1;
    }

    std::sort(poolPages.begin(), poolPages.end(), [](const pair<void*,uint64_t>& a, const pair<void*,uint64_t>& b) {
        return a.second < b.second;
    });

    int numPages = g_config.numHugePages;
    int MaxFuncOnlyBit = getMaxOnlyFuncBit();

    size_t group_size = 1;
    if (MaxFuncOnlyBit >= 30) {
        group_size <<= (MaxFuncOnlyBit + 1 - 30);
        numPages = ((numPages + group_size - 1) / group_size) * group_size;
        // numPages = (numPages / group_size) * group_size;
    }

    cout << "group size: " << group_size << endl;

    size_t group_bytes = group_size * HUGEPAGE_SIZE;

    vector<pair<void*,uint64_t>> usingPages;
    usingPages.reserve(numPages);


    vector<pair<void*,uint64_t>> unusedPages;
    unusedPages.reserve(g_config.poolSize - numPages);


    for (size_t i = 0; i < poolPages.size(); ++i) {
        if ((usingPages.size() >= numPages) ||
            (i + group_size - 1 > poolPages.size()) ||
            ((poolPages[i].second % group_bytes) != 0)
           ) {
            unusedPages.push_back(poolPages[i]);
            continue;
        }

        bool is_consecutive = true;
        for (size_t k = 1; k < group_size; ++k) {
            if (poolPages[i+k].second != poolPages[i].second + (k * HUGEPAGE_SIZE)) {
                is_consecutive = false;
                break;
            }
        }

        if (is_consecutive == false) {
            unusedPages.push_back(poolPages[i]);
            continue;
        }

        cout << "  [Found] Chunk at PAddr 0x" << hex << poolPages[i].second << dec
             << " (Size: " << group_size << ")" << endl;

        for (size_t j = 0; j < group_size; j++) {
            usingPages.push_back(poolPages[i+j]);
        }


        i += (group_size - 1);
    }

    if (usingPages.size() == 0) {
        cerr << " Cannot find enough Page Groups" << endl;
        for (auto& up : unusedPages) munmap(up.first, HUGEPAGE_SIZE);
        return -1;
    }


    vector<void*> hugePagePtrs;
    hugePagePtrs.reserve(usingPages.size());


    int align_bit = (getMaxBit()/4)*4 + 4;
    cout << "align_bit: " << align_bit << endl;
    size_t align_size = 1ULL << (align_bit - 30);
    size_t align_bytes = (align_size << 4) * HUGEPAGE_SIZE;
    size_t align_mask = align_bytes - 1;
    uintptr_t aligned_paddr = (uintptr_t)usingPages[0].second & align_mask;
    cout << "aligned_paddr: 0x" << hex << (uint64_t)aligned_paddr << dec << endl;
    size_t required_bytes = usingPages.size() * HUGEPAGE_SIZE;
    void* reserve = mmap(NULL, align_bytes * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uintptr_t base_vaddr = reinterpret_cast<uintptr_t>(reserve);
    uintptr_t aligned_vaddr = (base_vaddr & ~(align_mask)) | aligned_paddr;
    if ((base_vaddr & align_mask) > aligned_paddr) aligned_vaddr += align_bytes;

    munmap(reserve, aligned_vaddr - base_vaddr);
    munmap(reinterpret_cast<void*>(aligned_vaddr + required_bytes), (base_vaddr + align_bytes * 2) - (aligned_vaddr + required_bytes));


    for (int i = 0; i < usingPages.size(); i++) {
        munmap(usingPages[i].first, HUGEPAGE_SIZE);
        void* new_addr = mmap(reinterpret_cast<void*>(aligned_vaddr + HUGEPAGE_SIZE * i), HUGEPAGE_SIZE,
                                  PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB | MAP_FIXED, -1, 0);

        if (new_addr == MAP_FAILED) {
            cerr << "[ERROR] MAP_FIXED failed at " << hex << aligned_vaddr << endl;
            for (void* hp : hugePagePtrs) munmap(hp, HUGEPAGE_SIZE);
            for (auto& up : unusedPages) munmap(up.first, HUGEPAGE_SIZE);
            return -1;
        }

        volatile char* ptr = reinterpret_cast<volatile char*>(new_addr);
        ptr[0] = 1;

        hugePagePtrs.push_back(new_addr);
    }

    for (int i = 0; i < unusedPages.size(); i++) {
        munmap(unusedPages[i].first, HUGEPAGE_SIZE);
    }



    for (int i = 0; i < hugePagePtrs.size(); i++) {
        cout << "page: PA) 0x" << hex << getPhysicalAddr(hugePagePtrs[i]) << " VA) 0x" << (uint64_t)hugePagePtrs[i] << endl;
    }
    cout << "[Filter] Selection complete. Retained "<< dec << hugePagePtrs.size() << " pages." << endl;



    // Print Settings
    cout << "Benchmarks to run: ";
    for(const auto& b : target_benchmarks) cout << b << " ";
    cout << endl;
    cout << "Mask Settings:" << hex << endl;
    cout << "  Row Mask: 0x" << g_config.row_mask << endl;
    cout << "  Col Mask: 0x" << g_config.col_mask << endl;
    cout << dec;

    // [Init] 전역 변수 초기화
    g_config.num_ch   = calcNum(g_config.ch_masks);
    g_config.num_slot = calcNum(g_config.slot_masks);
    g_config.num_sc   = calcNum(g_config.sub_ch_masks);
    g_config.num_rk   = calcNum(g_config.rank_masks);
    g_config.num_bg   = calcNum(g_config.bg_masks);
    g_config.num_bank = calcNum(g_config.bank_masks);
    g_config.max_col_idx = 1ULL << countSetBits(g_config.col_mask);

    if (!g_config.seed_provided) {
        std::random_device rd;
        g_config.seed = rd();
    }
    g_gen.seed(g_config.seed);
    cout << "Using random seed: " << g_config.seed << endl;

    // classification
    perform_classification(hugePagePtrs);

    // 3. Run Benchmark Loop
    for (size_t b_idx = 0; b_idx < target_benchmarks.size(); ++b_idx) {
        string current_bench = target_benchmarks[b_idx];
        cout << "\n\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;
        cout << ">>> Running Benchmark [" << (b_idx+1) << "/" << target_benchmarks.size() << "]: " << current_bench << endl;
        cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;

        if (current_bench == "BW_ALL_RD") {
            cout << "=== Running BW_ALL_RD ===" << endl;

            vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank);

            shuffle(stream.begin(), stream.end(), g_gen);

            cout << "\n========================================================" << endl;
            cout << "                BW_ALL_RD Benchmark Results                " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_old(stream);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements     : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_ALL_OLD") {
            cout << "=== Running BW_ALL_OLD ===" << endl;

            vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank);

            cout << "\n========================================================" << endl;
            cout << "                BW_ALL_OLD Benchmark Results                " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_old(stream);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements     : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_ALL") {
            cout << "=== Running BW_ALL ===" << endl;

            pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHit(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second[0];
            int col_stride = pattern_pair.second[1];
            vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);

            cout << "\n========================================================" << endl;
            cout << "                BW_ALL Benchmark Results                " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_withPattern(stream, patterns);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements     : " << res.valid_count << endl;


            cout << "========================================================" << endl;
        }



        else if (current_bench == "BW_ALL_RM") {
            cout << "=== Running BW_ALL_RM ===" << endl;

            int col_stride = 1;
            pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank, col_stride);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second;
            vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride, col_stride);

            cout << "\n========================================================" << endl;
            cout << "              BW_ALL_RM Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_withPattern(stream, patterns);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "TEST") {
            cout << "=== Running TEST ===" << endl; //pattern, per cores, row hit

            
            cout << "\n========================================================" << endl;
            cout << "              TEST Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            bool is_hit = true;
            BenchResult res = measureBandwidth_withPattern_perCores(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank, is_hit);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "TEST1") {
            cout << "=== Running TEST1 ===" << endl; //pattern, per cores, row miss

            
            cout << "\n========================================================" << endl;
            cout << "              TEST1 Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            bool is_hit = false;
            BenchResult res = measureBandwidth_withPattern_perCores(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank, is_hit);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "TEST2") {
            cout << "=== Running TEST2 ===" << endl;    //pointer chasing, single trace, row hit

            vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank);
            // vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, 1, g_config.num_bg, g_config.num_bank);
            int col_stride = 1;
            // vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank,col_stride);
            // vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank,col_stride);


            // shuffle(stream.begin(), stream.end(), g_gen);

            int num_chains_per_cores = 32;
            int chain_stride = 1;

            cout << "\n========================================================" << endl;
            cout << "              TEST2 Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_PointerChasing(stream, num_chains_per_cores, chain_stride);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "TEST3") {
            cout << "=== Running TEST3 ===" << endl; //pointer chasing, single trace, row miss

            // vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank);
            // vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, 1, g_config.num_bg, g_config.num_bank);
            int col_stride = 1;
            vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank,col_stride);
            // vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank,col_stride);


            // shuffle(stream.begin(), stream.end(), g_gen);

            int num_chains_per_cores = 32;
            int chain_stride = 1;

            cout << "\n========================================================" << endl;
            cout << "              TEST3 Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_PointerChasing(stream, num_chains_per_cores, chain_stride);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }


        else if (current_bench == "TEST4") {
            cout << "=== Running TEST4 ===" << endl;

            // vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank);
            // vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, 1, g_config.num_bg, g_config.num_bank);
            int col_stride = 1;
            vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank,col_stride);
            // vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank,col_stride);


            // shuffle(stream.begin(), stream.end(), g_gen);

            int num_chains_per_threads = 64;
            int chain_stride = 1;

            cout << "\n========================================================" << endl;
            cout << "              TEST4 Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_PointerChasing(stream, num_chains_per_threads, chain_stride);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }


        else if (current_bench == "TEST5") {
            cout << "=== Running TEST5 ===" << endl; //pattern, per cores, row miss, 1rank

            
            cout << "\n========================================================" << endl;
            cout << "              TEST5 Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            bool is_hit = false;
            BenchResult res = measureBandwidth_withPattern_perCores(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank, is_hit);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }


        else if (current_bench == "TEST6") {
            cout << "=== Running TEST6 ===" << endl; //pointer chasing, single trace, row miss

            // vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank);
            // vector<void*> stream = createStreamRowHit(g_config.num_ch, g_config.num_slot, g_config.num_sc, 1, g_config.num_bg, g_config.num_bank);
            int col_stride = 1;
            vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank,col_stride);
            // vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank,col_stride);


            // shuffle(stream.begin(), stream.end(), g_gen);

            int num_chains_per_cores = 32;
            int chain_stride = 1;

            cout << "\n========================================================" << endl;
            cout << "              TEST6 Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_PointerChasing(stream, num_chains_per_cores, chain_stride);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_ALL_RM_OLD") {
            cout << "=== Running BW_ALL_RM_OLD ===" << endl;

            int col_stride = 1;
            vector<void*> stream = createStreamRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank,col_stride);



            cout << "\n========================================================" << endl;
            cout << "              BW_ALL_RM_OLD Benchmark Results               " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_old(stream);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time          : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements      : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_ALL_WO_RK") {
            cout << "=== Running BW_ALL_WO_RK (Single Rank Throughput) ===" << endl;

            pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHit(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second[0];
            int col_stride = pattern_pair.second[1];
            vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);
            cout << "\n========================================================" << endl;
            cout << "           BW_ALL_WO_RK Benchmark Results             " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_withPattern(stream, patterns);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements     : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }



        else if (current_bench == "BW_ALL_WO_RK_RM") {
            cout << "=== Running BW_ALL_WO_RK_RM (Single Rank Throughput) ===" << endl;

            pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second;
            vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride);

            cout << "\n========================================================" << endl;
            cout << "           BW_ALL_WO_RK_RM Benchmark Results             " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_withPattern(stream, patterns);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << " - Elements     : " << res.valid_count << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_SB") {
            cout << "=== Running BW_SB (Single Bank Throughput) ===" << endl;

            pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHit(1,1,1,1,1,1);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second[0];
            int col_stride = pattern_pair.second[1];
            vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);

            cout << "\n========================================================" << endl;
            cout << "                  BW_SB Benchmark Results                  " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_withPattern(stream,patterns);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << "========================================================" << endl;

        }

        else if (current_bench == "BW_SB_RM") {
            cout << "=== Running BW_SB (Single Bank Throughput) ===" << endl;

            pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(1,1,1,1,1,1);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second;
            vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride);

            cout << "\n========================================================" << endl;
            cout << "                  BW_SB_RM Benchmark Results                  " << endl;
            cout << "========================================================" << endl;

            BenchResult res = measureBandwidth_withPattern(stream,patterns);

            cout << fixed << setprecision(3);
            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << " - Avg Latency: " << res.latency_ns << " ns" << endl;
            cout << " - Time         : " << setprecision(6) << res.elapsed_sec << " sec" << endl;
            cout << "========================================================" << endl;

        }

        else if (current_bench == "BW_DG") {
            cout << "=== Running BW_DG (Interleaving Bandwidth Test) ===" << endl;

            cout << "\n========================================================" << endl;
            cout << "                BW_DG Benchmark Results                  " << endl;
            cout << "========================================================" << endl;

            cout << "\n--------------------------------------------------------" << endl;
            cout << "\tDifferent Group Test" << endl;

            pair<vector<uint64_t>,vector<int>> DG_pattern_pair = getPatternsRowHit(1,1,1,1,2,1);
            vector<uint64_t> DG_patterns = DG_pattern_pair.first;
            int DG_row_stride = DG_pattern_pair.second[0];
            int DG_col_stride = DG_pattern_pair.second[1];
            vector<uint64_t> DG_stream = createBaseAddrsRowHit(DG_row_stride,DG_col_stride);

            BenchResult DG_res = measureBandwidth_withPattern(DG_stream,DG_patterns);
            cout << " - Throughput : " << DG_res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << DG_res.usage << " %" << endl;

            cout << "--------------------------------------------------------" << endl;
            cout << "\tSame Group Test" << endl;
            pair<vector<uint64_t>,vector<int>> SG_pattern_pair = getPatternsRowHit(1,1,1,1,1,2);
            vector<uint64_t> SG_patterns = SG_pattern_pair.first;
            int SG_row_stride = SG_pattern_pair.second[0];
            int SG_col_stride = SG_pattern_pair.second[1];
            vector<uint64_t> SG_stream = createBaseAddrsRowHit(SG_row_stride,SG_col_stride);

            BenchResult SG_res = measureBandwidth_withPattern(SG_stream,SG_patterns);
            cout << " - Throughput : " << SG_res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << SG_res.usage << " %" << endl;

            cout << "--------------------------------------------------------" << endl;

            double ratio = (SG_res.bw_gbs > 0) ? (DG_res.bw_gbs / SG_res.bw_gbs) : 0.0;
            cout << "   - Ratio (Diff / Same): " << ratio << "x" << endl;
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_DG_4") {
            cout << "=== Running BW_DG_4 (4-Bank Interleaving Test) ===" << endl;

            cout << "\n========================================================" << endl;
            cout << "                BW_DG_4 Benchmark Results                  " << endl;
            cout << "========================================================" << endl;

            cout << "\n--------------------------------------------------------" << endl;
            cout << "\tDifferent Group Test" << endl;

            pair<vector<uint64_t>,vector<int>> DG_pattern_pair = getPatternsRowHit(1,1,1,1,4,1);
            vector<uint64_t> DG_patterns = DG_pattern_pair.first;
            int DG_row_stride = DG_pattern_pair.second[0];
            int DG_col_stride = DG_pattern_pair.second[1];
            vector<uint64_t> DG_stream = createBaseAddrsRowHit(DG_row_stride,DG_col_stride);

            BenchResult DG_res = measureBandwidth_withPattern(DG_stream,DG_patterns);
            cout << " - Throughput : " << DG_res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << DG_res.usage << " %" << endl;

            cout << "--------------------------------------------------------" << endl;
            cout << "\tSame Group Test" << endl;
            pair<vector<uint64_t>,vector<int>> SG_pattern_pair = getPatternsRowHit(1,1,1,1,1,4);
            vector<uint64_t> SG_patterns = SG_pattern_pair.first;
            int SG_row_stride = SG_pattern_pair.second[0];
            int SG_col_stride = SG_pattern_pair.second[1];
            vector<uint64_t> SG_stream = createBaseAddrsRowHit(SG_row_stride,SG_col_stride);

            BenchResult SG_res = measureBandwidth_withPattern(SG_stream,SG_patterns);
            cout << " - Throughput : " << SG_res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << SG_res.usage << " %" << endl;

            cout << "--------------------------------------------------------" << endl;

            double ratio = (SG_res.bw_gbs > 0) ? (DG_res.bw_gbs / SG_res.bw_gbs) : 0.0;
            cout << "   - Ratio (Diff / Same): " << ratio << "x" << endl;
            cout << "========================================================" << endl;
        }


        else if (current_bench == "BW_DOUBLE_BANK") {
            cout << "=== Running BW_DOUBLE_BANK (Mask Verification) ===" << endl;
            cout << "Reference Bank: CH:0 SLOT:0 SC:0 RK:0 BG:0 B:0" << endl;
            cout << "Fix masks bit to 0 except for target mask." << endl;

            const vector<vector<uint64_t>> masks = {
                g_config.ch_masks, g_config.slot_masks, g_config.sub_ch_masks,
                g_config.rank_masks, g_config.bg_masks, g_config.bank_masks
            };
            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};
            const vector<string> types = {"CH", "SLOT", "SC", "RK", "BG", "BA"};

            cout << "========================================================" << endl;
            cout << "\tBW_DOUBLE_BANK Benchmark Results" << endl;
            cout << "========================================================" << endl;

            // 0:CH, 1:SLOT, 2:SC, 3:RK, 4:BG, 5:BANK
            for (int i = 0; i < 6; i++) {
                if (counts[i] <= 1) continue;
                cout << "--------------------------------------------------------" << endl;
                cout << "\t" << types[i] << " test" << endl;
                cout << "----------------------------" << endl;

                vector<vector<int>> targets(6);

                for (int j = 0; j < 6; j++) {
                    if (i != j) {
                        targets[j].resize(1,0);
                    }
                    else {
                        targets[j].resize(2,0);
                    }
                }

                for (int j = 0; (1U << j) < counts[i]; j++) {
                    targets[i][1] = (1U << j);
                    cout << "target mask: 0x" << hex << masks[i][j] << dec << endl;
                    for (int k = 0; k < 6; k++) {
                        if (k != i) {
                            cout << types[k] << ": 0 ";
                        }
                        else {
                            cout << types[k] << ": " << (1U << j) << " ";
                        }
                    }
                    cout << endl;

                    pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHitUseVectors(targets);
                    vector<uint64_t> patterns = pattern_pair.first;
                    int row_stride = pattern_pair.second[0];
                    int col_stride = pattern_pair.second[1];
                    vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);

                    BenchResult res = measureBandwidth_withPattern(stream,patterns);
                    cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
                    cout << " - Usage : " << res.usage << " %" << endl;
                    cout << "----------------------------" << endl;
                }
                cout << "--------------------------------------------------------" << endl;
            }
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_HALF_BANK") {
            cout << "=== Running BW_HALF_BANK (Mask Verification) ===" << endl;
            cout << "Reference Bank: CH:0 SLOT:0 SC:0 RK:0 BG:0 B:0" << endl;
            cout << "Fix one mask bit to 0" << endl;

            const vector<vector<uint64_t>> masks = {
                g_config.ch_masks, g_config.slot_masks, g_config.sub_ch_masks,
                g_config.rank_masks, g_config.bg_masks, g_config.bank_masks
            };
            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};
            const vector<string> types = {"CH", "SLOT", "SC", "RK", "BG", "BA"};

            cout << "========================================================" << endl;
            cout << "\tBW_HALF_BANK Benchmark Results" << endl;
            cout << "========================================================" << endl;

            // 0:CH, 1:SLOT, 2:SC, 3:RK, 4:BG, 5:BANK
            for (int i = 0; i < 6; i++) {
                if (counts[i] <= 1) continue;
                cout << "--------------------------------------------------------" << endl;
                cout << "\t" << types[i] << " test" << endl;
                cout << "----------------------------" << endl;

                vector<vector<int>> targets(6);

                for (int j = 0; j < 6; j++) {
                    if (i != j) {
                        targets[j].resize(counts[j],0);
                        for (int k = 0; k < counts[j]; k++) {
                            targets[j][k] = k;
                        }
                    }
                }

                for (int j = 0; (1U << j) < counts[i]; j++) {
                    targets[i].clear();
                    for (int k = 0; k < counts[i]; k++) {
                        if (((1U << j) & k) == 0) targets[i].push_back(k);
                    }
                    cout << "target mask: 0x" << hex << masks[i][j] << dec << endl;

                    for (int k = 0; k < 6; k++) {
                        if (k != i) {
                            cout << types[k] << ": 0 ";
                        }
                        else {
                            cout << types[k] << ": " << (1U << j) << " ";
                        }
                    }
                    cout << endl;

                    cout << "pushed: ";
                    for (int k = 0; k < targets[i].size(); k++) {
                        cout << targets[i][k] << ", ";
                    }
                    cout << endl;

                    pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHitUseVectors(targets);
                    vector<uint64_t> patterns = pattern_pair.first;
                    int row_stride = pattern_pair.second[0];
                    int col_stride = pattern_pair.second[1];
                    vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);

                    BenchResult res = measureBandwidth_withPattern(stream,patterns);
                    cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
                    cout << " - Usage : " << res.usage << " %" << endl;
                    cout << "----------------------------" << endl;
                }
                cout << "--------------------------------------------------------" << endl;
            }
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_HALF_BANK_WO_RK") {
            cout << "=== Running BW_HALF_BANK_WO_RK (Single Rank + Half Bank Interleaving) ===" << endl;

            const vector<vector<uint64_t>> masks = {
                g_config.ch_masks, g_config.slot_masks, g_config.sub_ch_masks,
                g_config.rank_masks, g_config.bg_masks, g_config.bank_masks
            };
            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, 1, g_config.num_bg, g_config.num_bank};
            const vector<string> types = {"CH", "SLOT", "SC", "RK", "BG", "BA"};

            cout << "========================================================" << endl;
            cout << "\tBW_HALF_BANK_WO_RK Benchmark Results" << endl;
            cout << "========================================================" << endl;

            // 0:CH, 1:SLOT, 2:SC, 3:RK, 4:BG, 5:BANK
            for (int i = 0; i < 6; i++) {
                if (counts[i] <= 1) continue;
                cout << "--------------------------------------------------------" << endl;
                cout << "\t" << types[i] << " test" << endl;
                cout << "----------------------------" << endl;

                vector<vector<int>> targets(6);

                for (int j = 0; j < 6; j++) {
                    if (i != j) {
                        targets[j].resize(counts[j],0);
                        for (int k = 0; k < counts[j]; k++) {
                            targets[j][k] = k;
                        }
                    }
                }

                for (int j = 0; (1U << j) < counts[i]; j++) {
                    targets[i].clear();
                    for (int k = 0; k < counts[i]; k++) {
                        if (((1U << j) & k) == 0) targets[i].push_back(k);
                    }
                    cout << "target mask: 0x" << hex << masks[i][j] << dec << endl;

                    for (int k = 0; k < 6; k++) {
                        if (k != i) {
                            cout << types[k] << ": 0 ";
                        }
                        else {
                            cout << types[k] << ": " << (1U << j) << " ";
                        }
                    }
                    cout << endl;

                    cout << "pushed: ";
                    for (int k = 0; k < targets[i].size(); k++) {
                        cout << targets[i][k] << ", ";
                    }
                    cout << endl;

                    pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHitUseVectors(targets);
                    vector<uint64_t> patterns = pattern_pair.first;
                    int row_stride = pattern_pair.second[0];
                    int col_stride = pattern_pair.second[1];
                    vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);

                    BenchResult res = measureBandwidth_withPattern(stream,patterns);
                    cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
                    cout << " - Usage : " << res.usage << " %" << endl;
                    cout << "----------------------------" << endl;
                }
                cout << "--------------------------------------------------------" << endl;
            }
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_HALF_BANK_RM") {
            cout << "=== Running BW_HALF_BANK (Mask Verification) ===" << endl;
            cout << "Reference Bank: CH:0 SLOT:0 SC:0 RK:0 BG:0 B:0" << endl;
            cout << "Fix one mask bit to 0" << endl;

            const vector<vector<uint64_t>> masks = {
                g_config.ch_masks, g_config.slot_masks, g_config.sub_ch_masks,
                g_config.rank_masks, g_config.bg_masks, g_config.bank_masks
            };
            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};
            const vector<string> types = {"CH", "SLOT", "SC", "RK", "BG", "BA"};

            cout << "========================================================" << endl;
            cout << "\tBW_HALF_BANK_RM Benchmark Results" << endl;
            cout << "========================================================" << endl;

            // 0:CH, 1:SLOT, 2:SC, 3:RK, 4:BG, 5:BANK
            for (int i = 0; i < 6; i++) {
                if (counts[i] <= 1) continue;
                cout << "--------------------------------------------------------" << endl;
                cout << "\t" << types[i] << " test" << endl;
                cout << "----------------------------" << endl;

                vector<vector<int>> targets(6);

                for (int j = 0; j < 6; j++) {
                    if (i != j) {
                        targets[j].resize(counts[j],0);
                        for (int k = 0; k < counts[j]; k++) {
                            targets[j][k] = k;
                        }
                    }
                }

                for (int j = 0; (1U << j) < counts[i]; j++) {
                    targets[i].clear();
                    for (int k = 0; k < counts[i]; k++) {
                        if (((1U << j) & k) == 0) targets[i].push_back(k);
                    }
                    cout << "target mask: 0x" << hex << masks[i][j] << dec << endl;

                    for (int k = 0; k < 6; k++) {
                        if (k != i) {
                            cout << types[k] << ": 0 ";
                        }
                        else {
                            cout << types[k] << ": " << (1U << j) << " ";
                        }
                    }
                    cout << endl;

                    cout << "pushed: ";
                    for (int k = 0; k < targets[i].size(); k++) {
                        cout << targets[i][k] << ", ";
                    }
                    cout << endl;

                    pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMissUseVectors(targets);
                    vector<uint64_t> patterns = pattern_pair.first;
                    int row_stride = pattern_pair.second;
                    vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride);

                    BenchResult res = measureBandwidth_withPattern(stream,patterns);
                    cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
                    cout << " - Usage : " << res.usage << " %" << endl;
                    cout << "----------------------------" << endl;
                }
                cout << "--------------------------------------------------------" << endl;
            }
            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_SINGLE_BA") {
            cout << "=== Running BW_SINGLE_BA ===" << endl;

            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_SINGLE_BA Benchmark Results" << endl;
            cout << "========================================================" << endl;


            pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHit(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,1);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second[0];
            int col_stride = pattern_pair.second[1];

            vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);

            BenchResult res = measureBandwidth_withPattern(stream,patterns);

            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;

            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_SINGLE_BA_RM") {
            cout << "=== Running BW_SINGLE_BA_RM ===" << endl;

            cout << "========================================================" << endl;
            cout << "\tBW_SINGLE_BA_RM Benchmark Results" << endl;
            cout << "========================================================" << endl;

            pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,1);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second;

            vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride);

            BenchResult res = measureBandwidth_withPattern(stream,patterns);

            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;

            cout << "========================================================" << endl;
        }


        else if (current_bench == "BW_SINGLE_BA_WO_RK_RM") {
            cout << "=== Running BW_SINGLE_BA_WO_RK_RM ===" << endl;

            cout << "========================================================" << endl;
            cout << "\tBW_SINGLE_BA_WO_RK_RM Benchmark Results" << endl;
            cout << "========================================================" << endl;

            pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,1);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second;

            vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride);

            BenchResult res = measureBandwidth_withPattern(stream,patterns);

            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;

            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_SINGLE_BA_OLD") {
            cout << "=== Running BW_SINGLE_BA_OLD ===" << endl;

            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_SINGLE_BA_OLD Benchmark Results" << endl;
            cout << "========================================================" << endl;

            // for (int i = 0; i < g_config.num_bank; i++) {
            for (int i = 0; i < 1; i++) {
                cout << "--------------------------------------------------------" << endl;
                cout << "\t" << "BA " << i << endl;
                cout << "----------------------------" << endl;

                vector<vector<int>> targets(6);

                for (int j = 0; j < 5; j++) {
                    targets[j].resize(counts[j],0);
                    for (int k = 0; k < counts[j]; k++) {
                        targets[j][k] = k;
                    }
                }

                targets[5].push_back(i);

                vector<void*> stream = createRowHitStreamUseVectors(targets);
                BenchResult res = measureBandwidth_old(stream);

                cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
                cout << " - Usage : " << res.usage << " %" << endl;
                cout << "----------------------------" << endl;
            }

            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_SINGLE_BA_RM_OLD") {
            cout << "=== Running BW_SINGLE_BA_RM_OLD ===" << endl;

            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_SINGLE_BA_RM_OLD Benchmark Results" << endl;
            cout << "========================================================" << endl;

            // for (int i = 0; i < g_config.num_bank; i++) {
            for (int i = 0; i < 1; i++) {
                cout << "--------------------------------------------------------" << endl;
                cout << "\t" << "BA " << i << endl;
                cout << "----------------------------" << endl;

                vector<vector<int>> targets(6);

                for (int j = 0; j < 5; j++) {
                    targets[j].resize(counts[j],0);
                    for (int k = 0; k < counts[j]; k++) {
                        targets[j][k] = k;
                    }
                }

                targets[5].push_back(i);

                vector<void*> stream = createRowMissStreamUseVectors(targets);
                BenchResult res = measureBandwidth_old(stream);

                cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
                cout << " - Usage : " << res.usage << " %" << endl;
                cout << "----------------------------" << endl;
            }

            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_SINGLE_BA_RD") {
            cout << "=== Running BW_SINGLE_BA_RD ===" << endl;

            const vector<vector<uint64_t>> masks = {
                g_config.ch_masks, g_config.slot_masks, g_config.sub_ch_masks,
                g_config.rank_masks, g_config.bg_masks, g_config.bank_masks
            };
            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_SINGLE_BA_RD Benchmark Results" << endl;
            cout << "========================================================" << endl;

            // for (int i = 0; i < g_config.num_bank; i++) {
            for (int i = 0; i < 1; i++) {
                cout << "--------------------------------------------------------" << endl;
                cout << "\t" << "BA " << i << endl;
                cout << "----------------------------" << endl;

                vector<vector<int>> targets(6);

                for (int j = 0; j < 5; j++) {
                    targets[j].resize(counts[j],0);
                    for (int k = 0; k < counts[j]; k++) {
                        targets[j][k] = k;
                    }
                }

                targets[5].push_back(i);

                vector<void*> stream = createRowMissStreamUseVectors(targets);
                shuffle(stream.begin(),stream.end(),g_gen);

                BenchResult res = measureBandwidth_old(stream);


                cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
                cout << " - Usage : " << res.usage << " %" << endl;
                cout << "----------------------------" << endl;
            }

            cout << "========================================================" << endl;
        }


        else if (current_bench == "BW_DOUBLE_BA") {
            cout << "=== Running BW_DOUBLE_BA ===" << endl;

            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_DOUBLE_BA Benchmark Results" << endl;
            cout << "========================================================" << endl;


            pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHit(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank/2);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second[0];
            int col_stride = pattern_pair.second[1];

            vector<uint64_t> stream = createBaseAddrsRowHit(row_stride,col_stride);

            BenchResult res = measureBandwidth_withPattern(stream,patterns);


            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;


            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_DOUBLE_BA_RM") {
            cout << "=== Running BW_DOUBLE_BA_RM ===" << endl;


            cout << "========================================================" << endl;
            cout << "\tBW_DOUBLE_BA_RM Benchmark Results" << endl;
            cout << "========================================================" << endl;

            pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,g_config.num_rk,g_config.num_bg,g_config.num_bank/2);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second;

            vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride);

            BenchResult res = measureBandwidth_withPattern(stream,patterns);



            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;


            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_DOUBLE_BA_WO_RK_RM") {
            cout << "=== Running BW_DOUBLE_BA_WO_RK_RM ===" << endl;


            cout << "========================================================" << endl;
            cout << "\tBW_DOUBLE_BA_WO_RK_RM Benchmark Results" << endl;
            cout << "========================================================" << endl;

            pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(g_config.num_ch,g_config.num_slot,g_config.num_sc,1,g_config.num_bg,g_config.num_bank/2);
            vector<uint64_t> patterns = pattern_pair.first;
            int row_stride = pattern_pair.second;

            vector<uint64_t> stream = createBaseAddrsRowMiss(row_stride);

            BenchResult res = measureBandwidth_withPattern(stream,patterns);



            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;


            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_DOUBLE_BA_OLD") {
            cout << "=== Running BW_DOUBLE_BA_OLD ===" << endl;

            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_DOUBLE_BA_OLD Benchmark Results" << endl;
            cout << "========================================================" << endl;

            vector<vector<int>> targets(6);

            for (int j = 0; j < 5; j++) {
                targets[j].resize(counts[j],0);
                for (int k = 0; k < counts[j]; k++) {
                    targets[j][k] = k;
                }
            }

            for (int j = 0; j < g_config.num_bank/2; j++) {
                targets[5].push_back(j);
            }

            vector<void*> stream = createRowHitStreamUseVectors(targets);
            BenchResult res = measureBandwidth_old(stream);

            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;


            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_DOUBLE_BA_RM_OLD") {
            cout << "=== Running BW_DOUBLE_BA_RM_OLD ===" << endl;

            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_DOUBLE_BA_RM_OLD Benchmark Results" << endl;
            cout << "========================================================" << endl;

            vector<vector<int>> targets(6);

            for (int j = 0; j < 5; j++) {
                targets[j].resize(counts[j],0);
                for (int k = 0; k < counts[j]; k++) {
                    targets[j][k] = k;
                }
            }

            for (int j = 0; j < g_config.num_bank/2; j++) {
                targets[5].push_back(j);
            }

            vector<void*> stream = createRowMissStreamUseVectors(targets);
            BenchResult res = measureBandwidth_old(stream);

            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;


            cout << "========================================================" << endl;
        }

        else if (current_bench == "BW_DOUBLE_BA_RD") {
            cout << "=== Running BW_DOUBLE_BA_RD ===" << endl;

            const vector<vector<uint64_t>> masks = {
                g_config.ch_masks, g_config.slot_masks, g_config.sub_ch_masks,
                g_config.rank_masks, g_config.bg_masks, g_config.bank_masks
            };
            const int counts[] = {g_config.num_ch, g_config.num_slot, g_config.num_sc, g_config.num_rk, g_config.num_bg, g_config.num_bank};

            cout << "========================================================" << endl;
            cout << "\tBW_DOUBLE_BA_RD Benchmark Results" << endl;
            cout << "========================================================" << endl;

            vector<vector<int>> targets(6);

            for (int j = 0; j < 5; j++) {
                targets[j].resize(counts[j],0);
                for (int k = 0; k < counts[j]; k++) {
                    targets[j][k] = k;
                }
            }

            for (int j = 0; j < g_config.num_bank/2; j++) {
                targets[5].push_back(j);
            }

            vector<void*> stream = createRowMissStreamUseVectors(targets);
            shuffle(stream.begin(),stream.end(),g_gen);

            BenchResult res = measureBandwidth_old(stream);


            cout << " - Throughput : " << res.bw_gbs << " GB/s" << endl;
            cout << " - Usage : " << res.usage << " %" << endl;
            cout << "----------------------------" << endl;

            cout << "========================================================" << endl;
        }


    }

    // Cleanup RowData Memory
    for(int c=0; c<MAX_CH; c++)
    for(int s=0; s<MAX_SLOT; s++)
    for(int sc=0; sc<MAX_SC; sc++)
    for(int r=0; r<MAX_RK; r++)
    for(int bg=0; bg<MAX_BG; bg++)
    for(int b=0; b<MAX_BANK; b++) {
        for(auto* ptr : addr[c][s][sc][r][bg][b]) {
            RowData::destroy(ptr);
        }
    }

    for (void* ptr : hugePagePtrs) {
        munmap(ptr, HUGEPAGE_SIZE);
    }

    return 0;
}
