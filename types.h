#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <x86intrin.h>
#include <emmintrin.h>  // SSE2
#include <immintrin.h>  // BMI2 (_pext_u64)
#include <random>
#include <getopt.h>     // getopt_long
#include <algorithm>
#include <iomanip>
#include <omp.h>
#include <set>
#include <pthread.h>
#include <sched.h>

using namespace std;


static const size_t BASE_PAGE_SIZE = 4096;
static const size_t HUGEPAGE_SIZE = 1UL * 1024 * 1024 * 1024; // 1GB

#ifndef MAP_HUGE_1GB
  #ifndef MAP_HUGE_SHIFT
    #define MAP_HUGE_SHIFT 26
  #endif
  #define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)
#endif

// 하드웨어 계층별 최대 크기 상수
static const int MAX_CH = 4;
static const int MAX_SLOT = 4;
static const int MAX_SC = 2;
static const int MAX_RK = 2;
static const int MAX_BG = 8;
static const int MAX_BANK = 4;



// 설정 정보 저장
struct Config {
    string bench = "";
    int numHugePages = 0;
    int poolSize = 0;

    double cpu_period = 0.5; // ns per cycle (= 1 / cpu_freq_ghz)

    double DRAM_BW = 38.4;

    int NUMA_node = 0;
    int NUMA_stride = 2;

    // Masks
    vector<uint64_t> ch_masks;
    vector<uint64_t> slot_masks;
    vector<uint64_t> sub_ch_masks;
    vector<uint64_t> rank_masks;
    vector<uint64_t> bg_masks;
    vector<uint64_t> bank_masks;
    uint64_t row_mask = 0;
    uint64_t col_mask = 0;

    // Options
    unsigned int seed = 0;
    bool seed_provided = false;

    int min_cores = 1;
    int num_chains_per_core = 4;

    // Debug & Chunk & Core Test
    bool test_all_cores = false;
    bool verbose_output = false;
    int chunk_size = 1;

    // 런타임에 마스크로부터 계산되는 DRAM 구조 정보
    int num_ch = 1;
    int num_slot = 1;
    int num_sc = 1;
    int num_rk = 1;
    int num_bg = 1;
    int num_bank = 1;
    uint64_t max_col_idx = 0;
    uint64_t num_rows = 0;

    // 분류 작업 상태 플래그
    bool is_classified = false;
};

// 뱅크 계층 정보
struct BankInfo {
    int ch, slot, sc, rk, bg, bank, row, col;
};

// [Optimized] Row 데이터 구조체 - Flexible Array Member 사용
// 메모리 할당 횟수 최소화 및 캐시 지역성 극대화
struct RowData {
    uint32_t row_addr;
    uint32_t col_count;    // 채워진 column 수 추적
    void* cols[]; // 가변 길이 배열 (Flexible Array Member)

    // 생성자 역할을 하는 정적 함수 (malloc 사용)
    static RowData* create(uint32_t r_addr, size_t col_size) {
        // 구조체 크기 + 포인터 배열 크기만큼 한 번에 할당
        size_t total_size = sizeof(RowData) + (sizeof(void*) * col_size);
        void* mem = std::malloc(total_size);
        if (!mem) return nullptr;

        RowData* ptr = static_cast<RowData*>(mem);
        ptr->row_addr = r_addr;
        ptr->col_count = 0;
        // 배열 영역 0으로 초기화
        std::memset(ptr->cols, 0, sizeof(void*) * col_size);
        return ptr;
    }

    // 소멸자 역할을 하는 정적 함수
    static void destroy(RowData* ptr) {
        std::free(ptr);
    }
};

// 분류 작업을 위한 임시 데이터 튜플
struct Entry {
    uint32_t row;
    uint16_t col;
    void* addr;
};

// 벤치마크 결과 반환용
struct BenchResult {
    double elapsed_sec;
    double bw_gbs;
    double usage;
    double latency_ns;
    long long valid_count;
};

// 페이지 정렬 및 필터링을 위한 헬퍼 구조체
struct PageInfo {
    void* vaddr;
    uint64_t paddr;
};


// 전역 변수 extern 선언
extern Config g_config;
extern vector<RowData*> addr[MAX_CH][MAX_SLOT][MAX_SC][MAX_RK][MAX_BG][MAX_BANK];
extern std::mt19937 g_gen;
