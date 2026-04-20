#pragma once

#include "types.h"

static const set<string> IMPLEMENTED_BENCHMARKS = {
    "BW_ALL_RD","BW_ALL", "BW_ALL_OLD", "BW_ALL_RM", "BW_ALL_RM_OLD", "BW_ALL_WO_RK", "BW_ALL_WO_RK_RM",
    "BW_SB", "BW_SB_RM", "BW_DG", "BW_DG_4", "BW_DOUBLE_BANK", "BW_HALF_BANK", "BW_HALF_BANK_WO_RK", "BW_HALF_BANK_RM",
    "BW_SINGLE_BA", "BW_SINGLE_BA_RM", "BW_SINGLE_BA_WO_RK_RM", "BW_SINGLE_BA_OLD", "BW_SINGLE_BA_RM_OLD", "BW_SINGLE_BA_RD",
    "BW_DOUBLE_BA", "BW_DOUBLE_BA_RM", "BW_DOUBLE_BA_WO_RK_RM", "BW_DOUBLE_BA_OLD", "BW_DOUBLE_BA_RM_OLD", "BW_DOUBLE_BA_RD",
    "TEST", "TEST1", "TEST2", "TEST3", "TEST4", "TEST5", "TEST6",
    

    "BW_ALL_HIT_BASE_SINGLE", "BW_ALL_HIT_BASE_PERCORE", "BW_ALL_HIT_PT_SINGLE", "BW_ALL_HIT_PT_PERCORE", "BW_ALL_HIT_PC_SINGLE", "BW_ALL_HIT_PC_PERCORE",
    "BW_ALL_MISS_BASE_SINGLE", "BW_ALL_MISS_BASE_PERCORE", "BW_ALL_MISS_PT_SINGLE", "BW_ALL_MISS_PT_PERCORE", "BW_ALL_MISS_PC_SINGLE", "BW_ALL_MISS_PC_PERCORE",
    "BW_ALL_RAND_BASE_SINGLE", "BW_ALL_RAND_BASE_PERCORE", "BW_ALL_RAND_PT_SINGLE", "BW_ALL_RAND_PT_PERCORE", "BW_ALL_RAND_PC_SINGLE", "BW_ALL_RAND_PC_PERCORE",


    "BW_1R_HIT_BASE_SINGLE", "BW_1R_HIT_BASE_PERCORE", "BW_1R_HIT_PT_SINGLE", "BW_1R_HIT_PT_PERCORE", "BW_1R_HIT_PC_SINGLE", "BW_1R_HIT_PC_PERCORE",
    "BW_1R_MISS_BASE_SINGLE", "BW_1R_MISS_BASE_PERCORE", "BW_1R_MISS_PT_SINGLE", "BW_1R_MISS_PT_PERCORE", "BW_1R_MISS_PC_SINGLE", "BW_1R_MISS_PC_PERCORE",
    "BW_1R_RAND_BASE_SINGLE", "BW_1R_RAND_BASE_PERCORE", "BW_1R_RAND_PT_SINGLE", "BW_1R_RAND_PT_PERCORE", "BW_1R_RAND_PC_SINGLE", "BW_1R_RAND_PC_PERCORE",

};

inline int randInt(int n) {
    std::uniform_int_distribution<int> dist(0, n - 1);
    return dist(g_gen);
}
inline size_t randSize(size_t n) {
    std::uniform_int_distribution<size_t> dist(0, n - 1);
    return dist(g_gen);
}

// Parsing
vector<uint64_t> parseHexList(const string& input);
vector<string> parseBenchList(const string& input);

void print_help(const char* prog_name);
void print_result(BenchResult res);

// Physical address
uint64_t getPhysicalAddr(void* vaddr);

// DRAM address decoding
int applyXorFunctions(uint64_t physAddr, const vector<uint64_t>& masks);
BankInfo getBankFromPhysicalAddr(uint64_t physAddr);

// Bit utilities
int calcNum(const vector<uint64_t>& masks);
int countSetBits(uint64_t n);
int getMaxBit();
int getMaxOnlyFuncBit();


vector<int> get_thread_list();