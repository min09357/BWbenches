#pragma once

#include "types.h"



vector<void*> createStreamRowHit(int nch, int nslot, int nsc, int nrk, int nbg, int nba);
vector<void*> createStreamRowMiss(int nch, int nslot, int nsc, int nrk, int nbg, int nba, int col_stride = 1);



// Vector-based stream creators
vector<void*> createRowHitStreamUseVectors(const vector<vector<int>>& targets);
vector<void*> createRowMissStreamUseVectors(const vector<vector<int>>& targets, int col_stride = 1);



// Use Patterns
pair<vector<uint64_t>,vector<int>> getPatternsRowHit(int nch, int nslot, int nsc, int nrk, int nbg, int nba);
pair<vector<uint64_t>,int> getPatternsRowMiss(int nch, int nslot, int nsc, int nrk, int nbg, int nba, int col_stride = 1);
pair<vector<uint64_t>,vector<int>> getPatternsRowHitUseVectors(vector<vector<int>> targets);
pair<vector<uint64_t>,int> getPatternsRowMissUseVectors(vector<vector<int>> targets);


vector<uint64_t> createBaseAddrsRowHit(int row_stride, int col_stride, int num_stream = 1);
vector<uint64_t> createBaseAddrsRowMiss(int row_stride, int col_stride = 1, int num_stream = 1);










