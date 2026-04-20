#include "classification.h"
#include "utils.h"


void sanitize_banks() {
    vector<vector<RowData*>*> active_banks;
    for(int c=0; c<MAX_CH; c++)
    for(int s=0; s<MAX_SLOT; s++)
    for(int sc=0; sc<MAX_SC; sc++)
    for(int r=0; r<MAX_RK; r++)
    for(int bg=0; bg<MAX_BG; bg++)
    for(int b=0; b<MAX_BANK; b++) {
        if(!addr[c][s][sc][r][bg][b].empty()) {
            active_banks.push_back(&addr[c][s][sc][r][bg][b]);
        }
    }

    if(active_banks.empty()) {
        cerr << "[ERROR] No active banks found to sanitize." << endl;
        exit(1);
    }

    size_t min_rows = -1UL;
    size_t max_rows = 0;

    for (auto* bank : active_banks) {
        size_t sz = bank->size();
        if (sz < min_rows) min_rows = sz;
        if (sz > max_rows) max_rows = sz;
    }
    cout << "[Sanitize] Pre-clean Stats -> Banks: " << active_banks.size()
         << ", Min Rows: " << min_rows << ", Max Rows: " << max_rows << endl;

    long long removed_rows_total = 0;

    #pragma omp parallel for reduction(+:removed_rows_total)
    for (size_t i = 0; i < active_banks.size(); ++i) {
        auto* bank = active_banks[i];

        // Remove_if로 조건에 맞지 않는 Row 포인터를 뒤로 보냄
        auto new_end = std::remove_if(bank->begin(), bank->end(), [](RowData* r) {
            return r->col_count < g_config.max_col_idx;
        });

        // 삭제 대상인 RowData들의 메모리 해제 (free)
        for (auto it = new_end; it != bank->end(); ++it) {
            RowData::destroy(*it);
        }

        size_t original_size = bank->size();
        size_t new_size = std::distance(bank->begin(), new_end);
        removed_rows_total += (original_size - new_size);

        // 벡터 크기 줄임
        bank->erase(new_end, bank->end());
    }

    cout << "[Sanitize] Removed " << removed_rows_total << " incomplete rows." << endl;

    min_rows = -1UL;
    max_rows = 0;

    for (auto* bank : active_banks) {
        size_t sz = bank->size();
        if (sz < min_rows) min_rows = sz;
        if (sz > max_rows) max_rows = sz;
    }
    cout << "[Sanitize] Post-clean Stats -> Min Rows: " << min_rows << ", Max Rows: " << max_rows << endl;

    if (min_rows == 0 || min_rows == -1UL) {
        cerr << "[ERROR] No valid rows remaining after sanitization. Exiting." << endl;
        exit(1);
    }

    #pragma omp parallel for
    for (size_t i = 0; i < active_banks.size(); ++i) {
        auto* bank = active_banks[i];
        if (bank->size() > min_rows) {
            // 초과되는 Row들도 메모리 해제 필요
            for (size_t k = min_rows; k < bank->size(); k++) {
                RowData::destroy((*bank)[k]);
            }
            bank->resize(min_rows);
        }
    }

    cout << "[Sanitize] All banks truncated to match Min Rows: " << min_rows << endl;
}

// [Debug Helper] 뱅크 정렬 및 Row 일치 여부 검증
void verify_row_alignment() {
    cout << "\n[Verify] Checking Row Sorting & Alignment across banks..." << endl;

    // 1. 활성화된 뱅크 포인터 수집
    vector<vector<RowData*>*> active_banks;
    vector<string> bank_names; // 에러 메시지용 좌표 저장

    for(int c=0; c<MAX_CH; c++)
    for(int s=0; s<MAX_SLOT; s++)
    for(int sc=0; sc<MAX_SC; sc++)
    for(int r=0; r<MAX_RK; r++)
    for(int bg=0; bg<MAX_BG; bg++)
    for(int b=0; b<MAX_BANK; b++) {
        if(!addr[c][s][sc][r][bg][b].empty()) {
            active_banks.push_back(&addr[c][s][sc][r][bg][b]);

            stringstream ss;
            ss << "C" << c << ":S" << s << ":SC" << sc << ":R" << r << ":BG" << bg << ":B" << b;
            bank_names.push_back(ss.str());
        }
    }

    if (active_banks.empty()) {
        cerr << "[Verify] FAIL: No active banks found!" << endl;
        return;
    }

    // 기준 뱅크 (첫 번째 활성 뱅크)
    auto* ref_bank = active_banks[0];
    string ref_name = bank_names[0];
    size_t num_rows = ref_bank->size();

    bool sort_ok = true;
    bool align_ok = true;

    // 병렬 처리 불가능 (순차 검증 필요)
    for (size_t i = 0; i < active_banks.size(); ++i) {
        auto* curr_bank = active_banks[i];
        string curr_name = bank_names[i];

        // Sanitize check (사이즈는 이미 sanitize_banks에서 맞췄겠지만 확인)
        if (curr_bank->size() != num_rows) {
            cerr << "[Verify] FAIL: Size mismatch! " << ref_name << "(" << num_rows << ") vs "
                 << curr_name << "(" << curr_bank->size() << ")" << endl;
            align_ok = false;
        }

        uint32_t prev_addr = 0;

        for (size_t r_idx = 0; r_idx < curr_bank->size(); ++r_idx) {
            uint32_t curr_addr = (*curr_bank)[r_idx]->row_addr;

            // 1. Check Sorting (오름차순 확인)
            if (r_idx > 0 && curr_addr <= prev_addr) {
                cerr << "[Verify] FAIL: Sorting Error in " << curr_name
                     << " at index " << r_idx << ". Prev: " << prev_addr << ", Curr: " << curr_addr << endl;
                sort_ok = false;
            }
            prev_addr = curr_addr;

            // 2. Check Alignment (기준 뱅크와 Row 주소 비교)
            // 인덱스 범위 내일 때만 비교
            if (r_idx < num_rows) {
                uint32_t ref_addr = (*ref_bank)[r_idx]->row_addr;
                if (curr_addr != ref_addr) {
                    cerr << "[Verify] FAIL: Row Mismatch! Index " << r_idx << endl;
                    cerr << "    Ref (" << ref_name << "): " << ref_addr << endl;
                    cerr << "    Cur (" << curr_name << "): " << curr_addr << endl;
                    align_ok = false;

                    // 에러가 너무 많이 뜨지 않도록 첫 에러에서 Break 하려면 아래 주석 해제
                    // break;
                }
            }
        }
        if (!sort_ok || !align_ok) break; // 에러 발견 시 중단
    }

    if (sort_ok && align_ok) {
        cout << "[Verify] PASS: All " << active_banks.size() << " active banks have "
             << num_rows << " rows, sorted and perfectly aligned." << endl;
    } else {
        cout << "[Verify] FINISHED WITH ERRORS." << endl;
        // 필요하다면 여기서 exit(1);
    }
}

// [Optimized] Memory Classification Wrapper
// Map 제거, Lock-Free 병합, 고정 배열 구조체 사용
void perform_classification(const vector<void*>& hugePagePtrs) {
    if (g_config.is_classified) {
        cout << "[Info] Memory already classified. Skipping." << endl;
        return;
    }

    cout << "\n========================================================" << endl;
    cout << "             STARTING MEMORY CLASSIFICATION             " << endl;
    cout << "========================================================" << endl;

    cout << "Configuration Detected:" << endl;
    cout << " CH:" << g_config.num_ch << " Slot:" << g_config.num_slot << " SC:" << g_config.num_sc
         << " RK:" << g_config.num_rk << " BG:" << g_config.num_bg << " Bank:" << g_config.num_bank << endl;
    cout << " Max Col Index Size: " << g_config.max_col_idx << endl;
    cout << "Start classifying hugepages..." << endl;
    cout << "  [Info] Classification using " << omp_get_max_threads() << " threads." << endl;

    double start_time = omp_get_wtime();

    // Flattened Bank Index를 계산하기 위한 구조체
    struct BankIdx { int c, s, sc, r, bg, b; };
    vector<BankIdx> all_target_banks;
    for(int c=0; c<g_config.num_ch; c++)
    for(int s=0; s<g_config.num_slot; s++)
    for(int sc=0; sc<g_config.num_sc; sc++)
    for(int r=0; r<g_config.num_rk; r++)
    for(int bg=0; bg<g_config.num_bg; bg++)
    for(int b=0; b<g_config.num_bank; b++) {
        all_target_banks.push_back({c,s,sc,r,bg,b});
    }

    // 전역 버퍼: 쓰레드별, 뱅크별로 데이터를 단순 수집 (Map 사용 안함)
    // local_buffers[ThreadID][CH][SLOT]... 구조는 복잡하므로,
    // 쓰레드별로 'Flat Bank Index'를 사용하여 접근하도록 함.
    // Memory overhead: vector header * threads * total_banks (Small)
    int max_threads = omp_get_max_threads();
    int total_banks = all_target_banks.size();

    // [Heap Allocation] avoid stack overflow
    // buffers[thread_id][bank_flat_idx]
    vector<vector<vector<Entry>>> thread_buffers(max_threads);
    for(int i=0; i<max_threads; i++) {
        thread_buffers[i].resize(total_banks);
    }

    // Step 1: Parallel Collection (No Locks, No Maps, No Allocations of RowData)
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        // pre-calculate strides for flattening
        // indices: c, s, sc, r, bg, b
        // simple mapping: using 6-dim array access or flattened logic.
        // For speed, let's just stick to accessing thread_buffers[tid][flat_idx].
        // We need a helper to get flat_idx fast.

        #pragma omp for nowait
        for (int i = 0; i < hugePagePtrs.size(); i++) {
            char* base = static_cast<char*>(hugePagePtrs[i]);
            uint64_t basePhysAddr = getPhysicalAddr(base);
            if (basePhysAddr == 0) continue;

            for (size_t offset = 0; offset < HUGEPAGE_SIZE; offset += 64) {
                uint64_t pAddr = basePhysAddr + offset;
                void* currAddr = base + offset;

                BankInfo b = getBankFromPhysicalAddr(pAddr);
                // Bounds check
                if (b.ch >= g_config.num_ch || b.slot >= g_config.num_slot || b.sc >= g_config.num_sc ||
                    b.rk >= g_config.num_rk || b.bg >= g_config.num_bg || b.bank >= g_config.num_bank) continue;

                // Calculate flat index manually to avoid nested lookups
                int flat_idx = b.ch;
                flat_idx = flat_idx * g_config.num_slot + b.slot;
                flat_idx = flat_idx * g_config.num_sc + b.sc;
                flat_idx = flat_idx * g_config.num_rk + b.rk;
                flat_idx = flat_idx * g_config.num_bg + b.bg;
                flat_idx = flat_idx * g_config.num_bank + b.bank;

                // Just push simple tuple. Very Fast.
                if (b.col < g_config.max_col_idx) {
                    thread_buffers[tid][flat_idx].push_back({(uint32_t)b.row, (uint16_t)b.col, currAddr});
                }
            }
        }
    } // End Parallel Collection

    // Step 2: Parallel Merge & Construct (Parallelize by Banks)
    // 각 뱅크는 독립적이므로 락이 필요 없습니다.
    #pragma omp parallel for schedule(dynamic)
    for (int idx = 0; idx < total_banks; idx++) {
        const auto& bi = all_target_banks[idx];

        // 1. Gather all entries for this bank from all threads
        vector<Entry> merged_entries;
        size_t total_size = 0;
        for(int t=0; t<max_threads; t++) {
            total_size += thread_buffers[t][idx].size();
        }
        if (total_size == 0) continue;

        merged_entries.reserve(total_size);
        for(int t=0; t<max_threads; t++) {
            merged_entries.insert(merged_entries.end(), thread_buffers[t][idx].begin(), thread_buffers[t][idx].end());
            // Free memory of thread buffer early
            vector<Entry>().swap(thread_buffers[t][idx]);
        }

        // 2. Sort by Row, then Col
        std::sort(merged_entries.begin(), merged_entries.end(),
            [](const Entry& a, const Entry& b) {
                if (a.row != b.row) return a.row < b.row;
                return a.col < b.col;
            });

        // 3. Construct RowData (Allocation happens here)
        auto& target_vec = addr[bi.c][bi.s][bi.sc][bi.r][bi.bg][bi.b];

        if (merged_entries.empty()) continue;

        RowData* currentRow = nullptr;
        uint32_t lastRowAddr = -1;

        for (const auto& entry : merged_entries) {
            if (entry.row != lastRowAddr) {
                // New Row Detected
                currentRow = RowData::create(entry.row, g_config.max_col_idx);
                target_vec.push_back(currentRow);
                lastRowAddr = entry.row;
            }
            // Fill column data (Flexible Array Access)
            if (currentRow) {
                if (currentRow->cols[entry.col] == nullptr) {
                    currentRow->col_count++;
                }
                currentRow->cols[entry.col] = entry.addr;
            }
        }
    }

    double end_time = omp_get_wtime();
    cout << "Classification finished in " << (end_time - start_time) << " sec." << endl;

    sanitize_banks();

    verify_row_alignment();

    g_config.num_rows = addr[0][0][0][0][0][0].size();

    g_config.is_classified = true;
    cout << "========================================================" << endl;
    cout << "             CLASSIFICATION COMPLETE                    " << endl;
    cout << "========================================================" << endl << endl;
}
