#pragma once

#include "types.h"

BenchResult measureBandwidth_old(const vector<void*>& stream);
BenchResult measureBandwidth_withPattern(const vector<uint64_t>& stream, const vector<uint64_t>& pattern);


BenchResult measureBandwidth_PointerChasing(const vector<void*>& stream, int num_chains_per_cores, int chain_stride = 1);


BenchResult measureBandwidth_withPattern_perCores(int nch, int nslot, int nsc, int nrk, int nbg, int nba, bool is_hit);