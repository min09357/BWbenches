#include "stream.h"
#include "utils.h"






vector<void*> createStreamRowHit(int nch, int nslot, int nsc, int nrk, int nbg, int nba) {
    int num_banks = nch * nslot * nsc * nrk * nbg * nba;

    vector<void*> stream(g_config.num_rows * g_config.max_col_idx * num_banks);

    int idx = 0;

    for (int r=0; r<g_config.num_rows; r++)
    for (int c=0; c<g_config.max_col_idx; c++)
    for(int s=0; s<nslot; s++)
    for(int ba=0; ba<nba; ba++)
    for(int bg=0; bg<nbg; bg++)
    for(int rk=0; rk<nrk; rk++)
    for(int sc=0; sc<nsc; sc++)
    for(int ch=0; ch<nch; ch++) {
        stream[idx++] = addr[ch][s][sc][rk][bg][ba][r]->cols[c];
    }

    return stream;
}


vector<void*> createStreamRowMiss(int nch, int nslot, int nsc, int nrk, int nbg, int nba, int col_stride) {
    int num_banks = nch * nslot * nsc * nrk * nbg * nba;

    vector<void*> stream(g_config.num_rows * g_config.max_col_idx * num_banks);

    int idx = 0;

    for (int c1=0; c1<g_config.max_col_idx/col_stride; c1++)
    for (int r=0; r<g_config.num_rows; r++)
    for (int c0=0; c0<col_stride; c0++)
    for(int s=0; s<nslot; s++)
    for(int ba=0; ba<nba; ba++)
    for(int bg=0; bg<nbg; bg++)
    for(int rk=0; rk<nrk; rk++)
    for(int sc=0; sc<nsc; sc++)
    for(int ch=0; ch<nch; ch++) {
        stream[idx++] = addr[ch][s][sc][rk][bg][ba][r]->cols[c1 * col_stride + c0];
    }

    return stream;
}










vector<void*> createRowHitStreamUseVectors(const vector<vector<int>>& targets) {

    int num_banks = 1;
    for (auto t : targets) num_banks *= t.size();

    vector<void*> stream(g_config.num_rows * g_config.max_col_idx * num_banks);

    int idx = 0;

    for (int r=0; r<g_config.num_rows; r++)
    for (int c=0; c<g_config.max_col_idx; c++)
    for(int s : targets[1])
    for(int b : targets[5])
    for(int bg : targets[4])
    for(int rk : targets[3])
    for(int sc : targets[2])
    for(int ch : targets[0]) {
        stream[idx++] = addr[ch][s][sc][rk][bg][b][r]->cols[c];
    }

    return stream;
}

vector<void*> createRowMissStreamUseVectors(const vector<vector<int>>& targets, int col_stride) {

    int num_banks = 1;
    for (auto t : targets) num_banks *= t.size();

    vector<void*> stream(g_config.num_rows * g_config.max_col_idx * num_banks);

    int idx = 0;

    for (int c1=0; c1<g_config.max_col_idx/col_stride; c1++)
    for (int r=0; r<g_config.num_rows; r++)
    for (int c0=0; c0<col_stride; c0++)
    for(int s : targets[1])
    for(int ba : targets[5])
    for(int bg : targets[4])
    for(int rk : targets[3])
    for(int sc : targets[2])
    for(int ch : targets[0]) {
        stream[idx++] = addr[ch][s][sc][rk][bg][ba][r]->cols[c1 * col_stride + c0];
    }

    return stream;
}






pair<vector<uint64_t>,vector<int>> getPatternsRowHit(int nch, int nslot, int nsc, int nrk, int nbg, int nba) {
    int threshold = 256;
    // int threshold = 64;
    int num_patterns = nch * nslot * nsc * nrk * nbg * nba;
    if (__builtin_popcount(num_patterns) != 1) {
        cerr << "Invalid pattern. Num patterns should be power of 2." << endl;
        exit(1);
    }
    int row_stride = 1;
    int col_stride = 1;
    if (num_patterns >= threshold) {
        cout << "num_patterns is " << num_patterns << endl;
    }
    else if (threshold/num_patterns <= g_config.max_col_idx) {
        cout << "num_patterns is changed from " << num_patterns << " to " << threshold << endl;
        col_stride = threshold / num_patterns;
        num_patterns = threshold;
        cout << "row_stride: " << row_stride << " col_stride: " << col_stride << endl;

    }
    else {
        cout << "num_patterns is changed from " << num_patterns << " to " << threshold << endl;
        row_stride = threshold/num_patterns/g_config.max_col_idx;
        col_stride = g_config.max_col_idx;
        num_patterns = threshold;
        cout << "row_stride: " << row_stride << " col_stride: " << col_stride << endl;
    }

    vector<uint64_t> patterns(num_patterns);
    vector<int> strides(2);
    strides[0] = row_stride;
    strides[1] = col_stride;

    uint64_t base_addr = (uint64_t)addr[0][0][0][0][0][0][0]->cols[0];
    int idx = 0;

    for (int r=0; r<row_stride; r++)
    for (int c=0; c<col_stride; c++)
    for(int s=0; s<nslot; s++)
    for(int b=0; b<nba; b++)
    for(int bg=0; bg<nbg; bg++)
    for(int rk=0; rk<nrk; rk++)
    for(int sc=0; sc<nsc; sc++)
    for(int ch=0; ch<nch; ch++) {
        uint64_t current_addr = (uint64_t)addr[ch][s][sc][rk][bg][b][r]->cols[c];
        patterns[idx++] = (base_addr ^ current_addr);
    }

    // int numinterleavebg = 8;

    // int nbg1 = (nbg > numinterleavebg) ? numinterleavebg : nbg;
    // int nbg2 = nbg / nbg1;

    // for (int r=0; r<row_stride; r++)
    // for(int bg2=0; bg2<nbg2; bg2++)
    // for (int c=0; c<col_stride; c++)
    // for(int s=0; s<nslot; s++)
    // for(int b=0; b<nba; b++)
    // for(int rk=0; rk<nrk; rk++)
    // for(int bg1=0; bg1<nbg1; bg1++)
    // for(int sc=0; sc<nsc; sc++)
    // for(int ch=0; ch<nch; ch++) {
    //     uint64_t current_addr = (uint64_t)addr[ch][s][sc][rk][bg2*nbg1+bg1][b][r]->cols[c];
    //     patterns[idx++] = (base_addr ^ current_addr);
    // }



    return {patterns, strides};
}




pair<vector<uint64_t>,int> getPatternsRowMiss(int nch, int nslot, int nsc, int nrk, int nbg, int nba, int col_stride) {
    int threshold = 256;
    // int threshold = 64;
    int num_patterns = nch * nslot * nsc * nrk * nbg * nba * col_stride;
    if (__builtin_popcount(num_patterns) != 1) {
        cerr << "Invalid pattern. Num patterns should be power of 2." << endl;
        exit(1);
    }
    int row_stride = 1;
    if (num_patterns >= threshold) {
        cout << "num_patterns is " << num_patterns << endl;
    }
    else {
        cout << "num_patterns is changed from " << num_patterns << " to " << threshold << endl;
        row_stride = threshold/num_patterns;
        num_patterns = threshold;
        cout << "row_stride: " << row_stride << endl;
    }

    vector<uint64_t> patterns(num_patterns);

    uint64_t base_addr = (uint64_t)addr[0][0][0][0][0][0][0]->cols[0];
    int idx = 0;


    for (int r=0; r<row_stride; r++)
    for (int c=0; c<col_stride; c++)
    for(int s=0; s<nslot; s++)
    for(int b=0; b<nba; b++)
    for(int bg=0; bg<nbg; bg++)
    for(int rk=0; rk<nrk; rk++)
    for(int sc=0; sc<nsc; sc++)
    for(int ch=0; ch<nch; ch++) {
        uint64_t current_addr = (uint64_t)addr[ch][s][sc][rk][bg][b][r]->cols[c];
        patterns[idx++] = (base_addr ^ current_addr);
    }

    // int numinterleavebg = 8;

    // int nbg1 = (nbg > numinterleavebg) ? numinterleavebg : nbg;
    // int nbg2 = nbg / nbg1;

    // for (int r=0; r<row_stride; r++)
    // for (int c=0; c<col_stride; c++)
    // for(int s=0; s<nslot; s++)
    // for(int bg2=0; bg2<nbg2; bg2++)
    // for(int b=0; b<nba; b++)
    // for(int rk=0; rk<nrk; rk++)
    // for(int bg1=0; bg1<nbg1; bg1++)
    // for(int sc=0; sc<nsc; sc++)
    // for(int ch=0; ch<nch; ch++) {
    //     uint64_t current_addr = (uint64_t)addr[ch][s][sc][rk][bg2*nbg1+bg1][b][r]->cols[c];
    //     patterns[idx++] = (base_addr ^ current_addr);
    // }


    return {patterns, row_stride};
}

pair<vector<uint64_t>,vector<int>> getPatternsRowHitUseVectors(vector<vector<int>> targets) {
    if (targets.size() != 6) {
        cerr << "Worng targets vector" << endl;
        exit(1);
    }

    int threshold = 256;
    int num_patterns = 1;
    for (auto t : targets) num_patterns *= t.size();

    if (__builtin_popcount(num_patterns) != 1) {
        cerr << "Invalid pattern. Num patterns should be power of 2." << endl;
        exit(1);
    }
    int row_stride = 1;
    int col_stride = 1;
    if (num_patterns >= threshold) {
        cout << "num_patterns is " << num_patterns << endl;
    }
    else if (threshold/num_patterns <= g_config.max_col_idx) {
        cout << "num_patterns is changed from " << num_patterns << " to " << threshold << endl;
        col_stride = threshold / num_patterns;
        num_patterns = threshold;
        cout << "row_stride: " << row_stride << " col_stride: " << col_stride << endl;

    }
    else {
        cout << "num_patterns is changed from " << num_patterns << " to " << threshold << endl;
        row_stride = threshold/num_patterns/g_config.max_col_idx;
        col_stride = g_config.max_col_idx;
        num_patterns = threshold;
        cout << "row_stride: " << row_stride << " col_stride: " << col_stride << endl;
    }

    vector<uint64_t> patterns(num_patterns);
    vector<int> strides(2);
    strides[0] = row_stride;
    strides[1] = col_stride;

    uint64_t base_addr = (uint64_t)addr[0][0][0][0][0][0][0]->cols[0];
    int idx = 0;

    for (int r=0; r<row_stride; r++)
    for (int c=0; c<col_stride; c++)
    for(int s : targets[1])
    for(int b : targets[5])
    for(int rk : targets[3])
    for(int bg : targets[4])
    for(int sc : targets[2])
    for(int ch : targets[0]) {
        uint64_t current_addr = (uint64_t)addr[ch][s][sc][rk][bg][b][r]->cols[c];
        patterns[idx++] = (base_addr ^ current_addr);
    }
    return {patterns, strides};
}

pair<vector<uint64_t>,int> getPatternsRowMissUseVectors(vector<vector<int>> targets) {
    if (targets.size() != 6) {
        cerr << "Worng targets vector" << endl;
        exit(1);
    }

    int threshold = 256;
    int num_patterns = 1;
    for (auto t : targets) num_patterns *= t.size();

    if (__builtin_popcount(num_patterns) != 1) {
        cerr << "Invalid pattern. Num patterns should be power of 2." << endl;
        exit(1);
    }
    int row_stride = 1;
    int col_stride = 1;
    if (num_patterns >= threshold) {
        cout << "num_patterns is " << num_patterns << endl;
    }
    else {
        cout << "num_patterns is changed from " << num_patterns << " to " << threshold << endl;
        row_stride = threshold/num_patterns;
        num_patterns = threshold;
        cout << "row_stride: " << row_stride << endl;
    }


    vector<uint64_t> patterns(num_patterns);

    uint64_t base_addr = (uint64_t)addr[0][0][0][0][0][0][0]->cols[0];
    int idx = 0;

    for (int r=0; r<row_stride; r++)
    for(int s : targets[1])
    for(int b : targets[5])
    for(int rk : targets[3])
    for(int bg : targets[4])
    for(int sc : targets[2])
    for(int ch : targets[0]) {
        uint64_t current_addr = (uint64_t)addr[ch][s][sc][rk][bg][b][r]->cols[0];
        patterns[idx++] = (base_addr ^ current_addr);
    }
    return {patterns, row_stride};
}


// vector<uint64_t> createBaseAddrsRowHit(int row_stride, int col_stride) {

//     if (col_stride > 1) {
//         cout << "stream size changed from " << g_config.num_rows * g_config.max_col_idx << " to " << g_config.num_rows / row_stride * g_config.max_col_idx / col_stride << endl;
//     }
//     vector<uint64_t> stream(g_config.num_rows / row_stride * g_config.max_col_idx / col_stride);

//     #pragma omp parallel for
//     for(size_t r=0; r<(g_config.num_rows/row_stride); r++) {
//         for(size_t c=0; c<(g_config.max_col_idx/col_stride); c++) {
//             size_t idx = r * (g_config.max_col_idx/col_stride) + c;
//             stream[idx] = (uint64_t)addr[0][0][0][0][0][0][r*row_stride]->cols[c*col_stride];
//         }
//     }

//     return stream;
// }

// vector<uint64_t> createBaseAddrsRowMiss(int row_stride, int col_stride) {

//     if (row_stride > 1) {
//         cout << "stream size changed from " << g_config.num_rows * g_config.max_col_idx << " to " << g_config.num_rows / row_stride * g_config.max_col_idx / col_stride << endl;
//     }
//     vector<uint64_t> stream(g_config.num_rows / row_stride * g_config.max_col_idx / col_stride);

//     #pragma omp parallel for
//     for(size_t c=0; c<g_config.max_col_idx / col_stride; c++) {
//         for(size_t r=0; r<(g_config.num_rows/row_stride); r++) {
//             size_t idx = c * (g_config.num_rows/row_stride) + r;
//             stream[idx] = (uint64_t)addr[0][0][0][0][0][0][r*row_stride]->cols[c*col_stride];
//         }
//     }

//     return stream;
// }













vector<uint64_t> createBaseAddrsRowHit(int row_stride, int col_stride, int num_stream) {

    

    int num_baserow_per_stream = (g_config.num_rows / num_stream) / row_stride;
    int num_basecol_per_row = g_config.max_col_idx/col_stride;

    vector<uint64_t> stream(num_baserow_per_stream * num_stream * num_basecol_per_row);


    #pragma omp parallel for
    for (size_t s=0; s<num_stream; s++) {
        for(size_t r=0; r<num_baserow_per_stream; r++) {
            for(size_t c=0; c<num_basecol_per_row; c++) {
                size_t idx = (s * num_baserow_per_stream + r) * num_basecol_per_row + c;
                stream[idx] = (uint64_t)addr[0][0][0][0][0][0][(s * num_baserow_per_stream + r)*row_stride]->cols[c*col_stride];
            }
        }
    }

    return stream;
}

vector<uint64_t> createBaseAddrsRowMiss(int row_stride, int col_stride, int num_stream) {

    

    int num_baserow_per_stream = (g_config.num_rows / num_stream) / row_stride;
    int num_basecol_per_row = g_config.max_col_idx/col_stride;


    vector<uint64_t> stream(num_baserow_per_stream * num_stream * num_basecol_per_row);

    #pragma omp parallel for
    for (size_t s=0; s<num_stream; s++) {
        for(size_t c=0; c<num_basecol_per_row; c++) {
            for(size_t r=0; r<num_baserow_per_stream; r++) {
                size_t idx = s * num_baserow_per_stream * num_basecol_per_row + c * num_baserow_per_stream + r;
                stream[idx] = (uint64_t)addr[0][0][0][0][0][0][(s * num_baserow_per_stream + r)*row_stride]->cols[c*col_stride];
            }
        }
    }

    return stream;
}
