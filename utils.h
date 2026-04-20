#pragma once

#include "types.h"

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
