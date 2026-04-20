#include "bandwidth.h"
#include "stream.h"

#include <cfloat>



BenchResult measureBandwidth_old(const vector<void*>& stream) {
    if (stream.empty()) return {0, 0, 0, 0, 0};

    int max_hardware_threads = omp_get_max_threads();
    vector<int> thread_counts;

    if (g_config.test_all_cores) {
        int core_stride = 1;
        int last_val = 1;
        // for (int t = 1; t < core_stride && t <= max_hardware_threads; t *= 2) {
        //     thread_counts.push_back(t);
        //     last_val = t;
        // }

        // int start_val = core_stride;
        int start_val = 4;
        if (max_hardware_threads >= start_val) {
            for (int t = start_val; t <= max_hardware_threads; t += core_stride) {
            // for (int t = start_val; t <= 12; t += core_stride) {
                thread_counts.push_back(t);
                last_val = t;
            }
        }

        // if (last_val != max_hardware_threads) {
        //     thread_counts.push_back(max_hardware_threads);
        // }

    } else {
        thread_counts.push_back(max_hardware_threads);
         
    }

    BenchResult best_res = {0, 0, 0, 0, 0};
    int best_core = 0;
    double best_dev = 0;

    if (g_config.verbose_output) {
        cout << "#cores\tAvgGBs\tUsage%\tAvgLat\tDeviate\tMaxGBs\tMinGBs\tMax%" << endl;
    }

    for (int t : thread_counts) {
        // Set thread count for this iteration
        omp_set_num_threads(t);

        size_t stream_size = stream.size();

        long long valid_elements = 0;
        #pragma omp parallel for reduction(+:valid_elements)
        for (size_t i = 0; i < stream.size(); i++) {
            if (stream[i] != nullptr) valid_elements++;
        }


        int iterations = 5;
        double total_time = 0;
        uint64_t total_cycles = 0;
        uint64_t min_cycles = -1;
        uint64_t max_cycles = 0;
        vector<uint64_t> start_cycles(t);
        vector<uint64_t> end_cycles(t);


        double total_times = 0;
        double min_time = -1;
        double max_time = 0;
        vector<double> start_times(t);
        vector<double> end_times(t);

        for (int i = 0; i < iterations; i++) {

            // Cache Flush
            #pragma omp parallel for
            for (size_t si = 0; si < stream.size(); si++) {
                if (stream[si]) _mm_clflush(stream[si]);
            }
            _mm_mfence();


            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                int nthreads = omp_get_num_threads();

                start_cycles[tid] = __rdtsc();

                for (size_t si = tid; si < stream_size; si += nthreads) {
                    *(volatile uint64_t*)stream[si];
                }

                // #pragma omp for schedule(static, g_config.chunk_size)
                // // #pragma omp for
                // for (size_t i = 0; i < stream_size; i++) {
                //     *(volatile uint64_t*)stream[i];
                // }

                _mm_lfence();

                end_cycles[tid] = __rdtsc();
                // end_times[tid] = omp_get_wtime();
            }



            uint64_t min_start_cycle = *std::min_element(start_cycles.begin(), start_cycles.end());
            uint64_t max_end_cycle = *std::max_element(end_cycles.begin(), end_cycles.end());

            uint64_t current_cycles = max_end_cycle - min_start_cycle;


            total_cycles += current_cycles;
            if (current_cycles > max_cycles) max_cycles = current_cycles;
            if (current_cycles < min_cycles) min_cycles = current_cycles;


            // double min_start_time = *std::min_element(start_times.begin(), start_times.end());
            // double max_end_time = *std::max_element(end_times.begin(), end_times.end());
            // double current_times = max_end_time - min_start_time;

            // total_times += current_times;
            // if (current_times > max_time) max_time = current_times;
            // if (current_times < min_time) min_time = current_times;

        }

        double avg_time = total_times / iterations;

        double avg_cycles = (double)total_cycles / iterations;
        double avg_nsec = avg_cycles *  g_config.cpu_period;
        double avg_sec = avg_nsec / 1e9;



        BenchResult current_res;
        // current_res.elapsed_sec = avg_time;
        current_res.elapsed_sec = avg_nsec / 1e9;
        current_res.valid_count = valid_elements;
        current_res.bw_gbs = 0;
        current_res.usage = 0;
        current_res.latency_ns = 0;

        if (current_res.elapsed_sec > 0 && valid_elements > 0) {
            double total_bytes = (double)valid_elements * 64.0;

            current_res.bw_gbs = total_bytes / avg_nsec;
            current_res.latency_ns = avg_nsec / valid_elements;

            // current_res.bw_gbs = (total_bytes / 1e9) / current_res.elapsed_sec;
            // current_res.latency_ns = (current_res.elapsed_sec * 1e9) / valid_elements;

            current_res.usage = current_res.bw_gbs/g_config.DRAM_BW*100;
        }

        if (g_config.verbose_output) {
            cout << t << "\t"
                 << fixed << setprecision(3) << current_res.bw_gbs
                 << fixed << setprecision(3) << "\t" << current_res.usage
                 << "\t" << current_res.latency_ns
                 << fixed << setprecision(3) << "\t" << (max_cycles - min_cycles)/avg_cycles
                 << "\t" << (double)valid_elements * 64.0 / (min_cycles *  g_config.cpu_period)
                 << "\t" << (double)valid_elements * 64.0 / (max_cycles *  g_config.cpu_period)
                 << "\t" << (double)valid_elements * 64.0 / (min_cycles *  g_config.cpu_period) / g_config.DRAM_BW * 100
                 << endl;
        }

        // Best Result Update (Maximize Throughput)
        if (current_res.bw_gbs > best_res.bw_gbs) {
            best_res = current_res;
            best_core = t;
            best_dev = (max_cycles - min_cycles)/avg_cycles;
        }
    }
    cout << " - best core: " << best_core << "\t with deviation " << best_dev << endl;

    return best_res;
}

BenchResult measureBandwidth_withPattern(const vector<uint64_t>& stream, const vector<uint64_t>& pattern) {
    if (stream.empty()) return {0, 0, 0, 0, 0};
    int max_hardware_threads = omp_get_max_threads();
    vector<int> thread_counts;

    if (g_config.test_all_cores) {
        int core_stride = 1;
        int last_val = 1;
        // for (int t = 1; t < core_stride && t <= max_hardware_threads; t *= 2) {
        //     thread_counts.push_back(t);
        //     last_val = t;
        // }

        // int start_val = core_stride;
        int start_val = 3;
        if (max_hardware_threads >= start_val) {
            for (int t = start_val; t <= max_hardware_threads; t += core_stride) {
            // for (int t = start_val; t <= 12; t += core_stride) {
                thread_counts.push_back(t);
                last_val = t;
            }
        }

        // if (last_val != max_hardware_threads) {
        //     thread_counts.push_back(max_hardware_threads);
        // }

    } else {
        thread_counts.push_back(max_hardware_threads);
         
    }

    BenchResult best_res = {0, 0, 0, 0, 0};
    int best_core = 0;
    double best_dev = 0;

    size_t stream_size = stream.size();
    size_t pattern_size = pattern.size();  // power of 2
    size_t pattern_mask = pattern_size - 1;
    int pattern_bits = __builtin_ctzll(pattern_size);
    size_t total_elements = stream_size * pattern_size;

    if (g_config.verbose_output) {
        cout << "#cores\tAvgGBs\tUsage%\tAvgLat\tDeviate\tMaxGBs\tMinGBs\tMax%" << endl;
    }

    for (int t : thread_counts) {
        // Set thread count for this iteration
        omp_set_num_threads(t);

        int iterations = 5;
        // int iterations = 10;
        double total_time = 0;
        uint64_t total_cycles = 0;
        uint64_t min_cycles = -1;
        uint64_t max_cycles = 0;
        double total_times = 0;
        double min_time = -1;
        double max_time = 0;
        vector<uint64_t> start_cycles(t);
        vector<uint64_t> end_cycles(t);

        vector<double> start_times(t);
        vector<double> end_times(t);


        for (int i = 0; i < iterations; i++) {
             // Cache Flush
            #pragma omp parallel for
            for (size_t si = 0; si < stream_size; si++) {
                for (size_t sj = 0; sj < pattern_size; sj++) {
                    void* target_addr = (void*)(stream[si] ^ pattern[sj]);
                    if (target_addr != nullptr) {
                        _mm_clflush((void*)target_addr);
                    }
                }
            }
            _mm_mfence();

            // save pattern in cache
            #pragma omp parallel
            {
                size_t local_dummy = 0;
                for (size_t pi = 0; pi < pattern_size; pi++) {
                    local_dummy += pattern[pi];
                }
                if (local_dummy == 0xdeadbeef) cout << "deadbeef" << endl;
            }

            _mm_mfence();

            // double st = omp_get_wtime();

            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                int nthreads = omp_get_num_threads();


                start_cycles[tid] = __rdtsc();

                // #pragma omp barrier

                for (size_t si = tid; si < total_elements; si+=nthreads) {
                    size_t stream_idx = si >> pattern_bits;
                    size_t pattern_idx = si & pattern_mask;
                    *(volatile uint64_t*)(stream[stream_idx] ^ pattern[pattern_idx]);
                }


                // /* In DDR4 Cascade Lake, this is better than upper */
                // // #pragma omp for collapse(2) schedule(static, g_config.chunk_size)
                // #pragma omp for
                // for (size_t i = 0; i < stream_size; i++) {
                //     for (size_t j = 0; j < pattern_size; j++) {
                //         *(volatile uint64_t*)(stream[i] ^ pattern[j]);
                //     }
                // }

                _mm_lfence();

                end_cycles[tid] = __rdtsc();
                // end_times[tid] = omp_get_wtime();

            }

            // double et = omp_get_wtime();



            _mm_lfence();
            // double end_time = omp_get_wtime();
            // uint64_t end_cycle = __rdtsc();

            // total_time += (end_time - start_time);
            // uint64_t current_cycles = end_cycle - start_cycle;

            uint64_t min_start_cycle = *std::min_element(start_cycles.begin(), start_cycles.end());
            uint64_t max_end_cycle = *std::max_element(end_cycles.begin(), end_cycles.end());
            uint64_t current_cycles = max_end_cycle - min_start_cycle;


            total_cycles += current_cycles;
            if (current_cycles > max_cycles) max_cycles = current_cycles;
            if (current_cycles < min_cycles) min_cycles = current_cycles;


            double min_start_time = *std::min_element(start_times.begin(), start_times.end());
            double max_end_time = *std::max_element(end_times.begin(), end_times.end());
            double current_times = max_end_time - min_start_time;

            total_times += current_times;
            if (current_times > max_time) max_time = current_times;
            if (current_times < min_time) min_time = current_times;
        }
        double avg_time = total_times / iterations;

        double avg_cycles = (double)total_cycles / iterations;
        double avg_nsec = avg_cycles *  g_config.cpu_period;
        double avg_sec = avg_nsec / 1e9;

        BenchResult current_res;

        current_res.elapsed_sec = avg_nsec / 1e9;    ////////////////

        // current_res.elapsed_sec = avg_time;

        current_res.valid_count = total_elements;
        current_res.bw_gbs = 0;
        current_res.usage = 0;
        current_res.latency_ns = 0;

        if (current_res.elapsed_sec > 0 && total_elements > 0) {
            double total_bytes = (double)total_elements * 64.0;


            // current_res.bw_gbs = (total_bytes / 1e9) / current_res.elapsed_sec;
            // current_res.latency_ns = (current_res.elapsed_sec * 1e9) / total_elements;

            current_res.latency_ns = avg_nsec / total_elements;
            current_res.bw_gbs = total_bytes  / avg_nsec;
            current_res.usage = current_res.bw_gbs/g_config.DRAM_BW*100;

        }

        if (g_config.verbose_output) {
            cout << t << "\t"
                 << fixed << setprecision(3) << current_res.bw_gbs
                 << fixed << setprecision(3) << "\t" << current_res.usage
                 << "\t" << current_res.latency_ns
                 << fixed << setprecision(3) << "\t" << (max_cycles - min_cycles)/avg_cycles
                 << "\t" << (double)total_elements * 64.0 / (min_cycles *  g_config.cpu_period)
                 << "\t" << (double)total_elements * 64.0 / (max_cycles *  g_config.cpu_period)
                 << "\t" << (double)total_elements * 64.0 / (min_cycles *  g_config.cpu_period) / g_config.DRAM_BW * 100
                 << endl;
        }

        if (current_res.bw_gbs > best_res.bw_gbs) {
            best_res = current_res;
            best_core = t;
            best_dev = (max_cycles - min_cycles)/avg_cycles;
        }
    }
    cout << " - best core: " << best_core << "\t with deviation " << best_dev << endl;

    return best_res;
}







BenchResult measureBandwidth_withPattern_perCores(int nch, int nslot, int nsc, int nrk, int nbg, int nba, bool is_hit) {

    vector<uint64_t> pattern;
    int row_stride;
    int col_stride;

    if (is_hit) {
        pair<vector<uint64_t>,vector<int>> pattern_pair = getPatternsRowHit(nch, nslot, nsc, nrk, nbg, nba);
        pattern = pattern_pair.first;
        row_stride = pattern_pair.second[0];
        col_stride = pattern_pair.second[1];
    } else {
        pair<vector<uint64_t>,int> pattern_pair = getPatternsRowMiss(nch, nslot, nsc, nrk, nbg, nba);
        pattern = pattern_pair.first;
        row_stride = pattern_pair.second;
        col_stride = 1; //change
    }




    int max_hardware_threads = omp_get_max_threads();
    vector<int> thread_counts;

    if (g_config.test_all_cores) {
        int core_stride = 1;
        int last_val = 1;
        // for (int t = 1; t < core_stride && t <= max_hardware_threads; t *= 2) {
        //     thread_counts.push_back(t);
        //     last_val = t;
        // }

        // int start_val = core_stride;
        int start_val = 3;
        if (max_hardware_threads >= start_val) {
            for (int t = start_val; t <= max_hardware_threads; t += core_stride) {
            // for (int t = start_val; t <= 12; t += core_stride) {
                thread_counts.push_back(t);
                last_val = t;
            }
        }

        // if (last_val != max_hardware_threads) {
        //     thread_counts.push_back(max_hardware_threads);
        // }

    } else {
        thread_counts.push_back(max_hardware_threads);
    }

    BenchResult best_res = {0, 0, 0, 0, 0};
    int best_core = 0;
    double best_dev = 0;

    size_t pattern_size = pattern.size();  // power of 2
    size_t pattern_mask = pattern_size - 1;
    int pattern_bits = __builtin_ctzll(pattern_size);
    

    if (g_config.verbose_output) {
        cout << "#cores\tAvgGBs\tUsage%\tAvgLat\tDeviate\tMaxGBs\tMinGBs\tMax%" << endl;
    }

    for (int t : thread_counts) {
        // Set thread count for this iteration
        omp_set_num_threads(t);

        vector<uint64_t> stream;

        if (is_hit) stream = createBaseAddrsRowHit(row_stride,col_stride,t);
        else stream = createBaseAddrsRowMiss(row_stride,col_stride,t);


        size_t stream_size = stream.size();
        size_t total_elements = stream_size * pattern_size;


        int iterations = 5;
        // int iterations = 10;
        double total_time = 0;
        uint64_t total_cycles = 0;
        uint64_t min_cycles = -1;
        uint64_t max_cycles = 0;
        double total_times = 0;
        double min_time = -1;
        double max_time = 0;
        vector<uint64_t> start_cycles(t);
        vector<uint64_t> end_cycles(t);

        vector<double> start_times(t);
        vector<double> end_times(t);


        for (int i = 0; i < iterations; i++) {
             // Cache Flush
            #pragma omp parallel for
            for (size_t si = 0; si < stream_size; si++) {
                for (size_t sj = 0; sj < pattern_size; sj++) {
                    void* target_addr = (void*)(stream[si] ^ pattern[sj]);
                    if (target_addr != nullptr) {
                        _mm_clflush((void*)target_addr);
                    }
                }
            }
            _mm_mfence();

            // save pattern in cache
            #pragma omp parallel
            {
                size_t local_dummy = 0;
                for (size_t pi = 0; pi < pattern_size; pi++) {
                    local_dummy += pattern[pi];
                }
                if (local_dummy == 0xdeadbeef) cout << "deadbeef" << endl;
            }

            _mm_mfence();

            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                int nthreads = omp_get_num_threads();

                start_cycles[tid] = __rdtsc();

                // for (size_t si = tid; si < total_elements; si+=nthreads) {
                //     size_t stream_idx = si >> pattern_bits;
                //     size_t pattern_idx = si & pattern_mask;
                //     *(volatile uint64_t*)(stream[stream_idx] ^ pattern[pattern_idx]);
                // }


                /* In DDR4 Cascade Lake, this is better than upper */
                // #pragma omp for collapse(2) schedule(static, g_config.chunk_size)
                #pragma omp for
                for (size_t i = 0; i < stream_size; i++) {
                    for (size_t j = 0; j < pattern_size; j++) {
                        *(volatile uint64_t*)(stream[i] ^ pattern[j]);
                    }
                }

                _mm_lfence();

                end_cycles[tid] = __rdtsc();
                // end_times[tid] = omp_get_wtime();

            }



            _mm_lfence();
            // double end_time = omp_get_wtime();
            // uint64_t end_cycle = __rdtsc();

            // total_time += (end_time - start_time);
            // uint64_t current_cycles = end_cycle - start_cycle;

            uint64_t min_start_cycle = *std::min_element(start_cycles.begin(), start_cycles.end());
            uint64_t max_end_cycle = *std::max_element(end_cycles.begin(), end_cycles.end());
            uint64_t current_cycles = max_end_cycle - min_start_cycle;


            total_cycles += current_cycles;
            if (current_cycles > max_cycles) max_cycles = current_cycles;
            if (current_cycles < min_cycles) min_cycles = current_cycles;


            double min_start_time = *std::min_element(start_times.begin(), start_times.end());
            double max_end_time = *std::max_element(end_times.begin(), end_times.end());
            double current_times = max_end_time - min_start_time;

            total_times += current_times;
            if (current_times > max_time) max_time = current_times;
            if (current_times < min_time) min_time = current_times;
        }
        double avg_time = total_times / iterations;

        double avg_cycles = (double)total_cycles / iterations;
        double avg_nsec = avg_cycles *  g_config.cpu_period;
        double avg_sec = avg_nsec / 1e9;

        BenchResult current_res;

        current_res.elapsed_sec = avg_nsec / 1e9;    ////////////////

        // current_res.elapsed_sec = avg_time;

        current_res.valid_count = total_elements;
        current_res.bw_gbs = 0;
        current_res.usage = 0;
        current_res.latency_ns = 0;

        if (current_res.elapsed_sec > 0 && total_elements > 0) {
            double total_bytes = (double)total_elements * 64.0;


            // current_res.bw_gbs = (total_bytes / 1e9) / current_res.elapsed_sec;
            // current_res.latency_ns = (current_res.elapsed_sec * 1e9) / total_elements;

            current_res.latency_ns = avg_nsec / total_elements;
            current_res.bw_gbs = total_bytes  / avg_nsec;
            current_res.usage = current_res.bw_gbs/g_config.DRAM_BW*100;

        }

        if (g_config.verbose_output) {
            cout << t << "\t"
                 << fixed << setprecision(3) << current_res.bw_gbs
                 << fixed << setprecision(3) << "\t" << current_res.usage
                 << "\t" << current_res.latency_ns
                 << fixed << setprecision(3) << "\t" << (max_cycles - min_cycles)/avg_cycles
                 << "\t" << (double)total_elements * 64.0 / (min_cycles *  g_config.cpu_period)
                 << "\t" << (double)total_elements * 64.0 / (max_cycles *  g_config.cpu_period)
                 << "\t" << (double)total_elements * 64.0 / (min_cycles *  g_config.cpu_period) / g_config.DRAM_BW * 100
                 << endl;
        }

        if (current_res.bw_gbs > best_res.bw_gbs) {
            best_res = current_res;
            best_core = t;
            best_dev = (max_cycles - min_cycles)/avg_cycles;
        }
    }
    cout << " - best core: " << best_core << "\t with deviation " << best_dev << endl;

    return best_res;
}






BenchResult measureBandwidth_PointerChasing(const vector<void*>& stream, int num_chains_per_cores, int chain_stride) {
    if (stream.empty()) return {0, 0, 0, 0, 0};
    int max_hardware_threads = omp_get_max_threads();
    vector<int> thread_counts;

    if (g_config.test_all_cores) {
        int core_stride = 1;
        int last_val = 1;
        // for (int t = 1; t < core_stride && t <= max_hardware_threads; t *= 2) {
        //     thread_counts.push_back(t);
        //     last_val = t;
        // }

        // int start_val = core_stride;
        int start_val = 3;
        if (max_hardware_threads >= start_val) {
            for (int t = start_val; t <= max_hardware_threads; t += core_stride) {
            // for (int t = start_val; t <= 12; t += core_stride) {
                thread_counts.push_back(t);
                last_val = t;
            }
        }

        // if (last_val != max_hardware_threads) {
        //     thread_counts.push_back(max_hardware_threads);
        // }

    } else {
        thread_counts.push_back(max_hardware_threads);
    }

    BenchResult best_res = {0, 0, 0, 0, 0};
    int best_core = 0;
    double best_dev = 0;

    size_t stream_size = stream.size();
    size_t total_elements;

    if (g_config.verbose_output) {
        cout << "#cores\tAvgGBs\tUsage%\tAvgLat\tDeviate\tMaxGBs\tMinGBs\tMax%" << endl;
    }

    for (int t : thread_counts) {
        // Set thread count for this iteration
        omp_set_num_threads(t);

        vector<vector<void*>> starting_node(t, vector<void*>(num_chains_per_cores));

        // for (int c = 0; c < num_chains_per_cores; c++) {
        //     for (int tid = 0; tid < t; tid++) {
        //         starting_node[tid][c] = stream[c * t + tid];
        //         // starting_node[tid][c] = stream[tid * num_chains_per_cores + c];
        //     }
        // }

        int idx = 0;
        for (int c = 0; c < num_chains_per_cores; c += chain_stride) {
            for (int tid = 0; tid < t; tid++) {
                for (int s = 0; (s < chain_stride) && ((c + s) < num_chains_per_cores); s++) {
                    starting_node[tid][c + s] = stream[idx];
                    idx++;
                }
            }
        }






        #pragma omp parallel for
        for (int i = 0; i <= (stream_size - t * num_chains_per_cores * 2); i += t * num_chains_per_cores) {
            for (int c = 0; c < num_chains_per_cores; c++) {
                for (int tid = 0; tid < t; tid++) {
                    size_t current_idx = i + c * t + tid;
                    *(void**)stream[current_idx] = stream[current_idx + t * num_chains_per_cores];
                }
            }
        }

        int remain = stream_size % (t * num_chains_per_cores);
        int remaining_start = stream_size - (t * num_chains_per_cores) - remain;

        for (int i = 0; i < (t * num_chains_per_cores); i++) {
            *(void**)stream[remaining_start + i] = stream[i];   // ignore tail
        }

        size_t chain_size = stream_size / (t * num_chains_per_cores);
        total_elements = chain_size * (t * num_chains_per_cores); 



        int iterations = 5;
        // int iterations = 10;
        double total_time = 0;
        uint64_t total_cycles = 0;
        uint64_t min_cycles = -1;
        uint64_t max_cycles = 0;
        double total_times = 0;
        double min_time = -1;
        double max_time = 0;
        vector<uint64_t> start_cycles(t);
        vector<uint64_t> end_cycles(t);

        vector<double> start_times(t);
        vector<double> end_times(t);


        for (int i = 0; i < iterations; i++) {
             // Cache Flush
            #pragma omp parallel for
            for (size_t si = 0; si < stream_size; si++) {
                _mm_clflush(stream[si]);
            }
            _mm_mfence();

            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                int nthreads = omp_get_num_threads();

                vector<void*> current_addr = starting_node[tid];


                start_cycles[tid] = __rdtsc();

                for (int step = 0; step < chain_size; step++) {
                    for (int c = 0; c < num_chains_per_cores; c++) {
                        current_addr[c] = *(void**)current_addr[c];
                    }
                }

                _mm_lfence();

                end_cycles[tid] = __rdtsc();
                // end_times[tid] = omp_get_wtime();

                _mm_lfence();
                uintptr_t sink = 0;
                for (int c = 0; c < num_chains_per_cores; c++) {
                    if (current_addr[c] != starting_node[tid][c]) {
                        cerr << "[WARN] Thread " << tid << " chain " << c << " did not return to starting node!" << endl;
                    }
                }
                asm volatile("" : : "r"(sink) : "memory");

            }



            _mm_lfence();
            // double end_time = omp_get_wtime();
            // uint64_t end_cycle = __rdtsc();

            // total_time += (end_time - start_time);
            // uint64_t current_cycles = end_cycle - start_cycle;

            uint64_t min_start_cycle = *std::min_element(start_cycles.begin(), start_cycles.end());
            uint64_t max_end_cycle = *std::max_element(end_cycles.begin(), end_cycles.end());
            uint64_t current_cycles = max_end_cycle - min_start_cycle;


            total_cycles += current_cycles;
            if (current_cycles > max_cycles) max_cycles = current_cycles;
            if (current_cycles < min_cycles) min_cycles = current_cycles;


            double min_start_time = *std::min_element(start_times.begin(), start_times.end());
            double max_end_time = *std::max_element(end_times.begin(), end_times.end());
            double current_times = max_end_time - min_start_time;

            total_times += current_times;
            if (current_times > max_time) max_time = current_times;
            if (current_times < min_time) min_time = current_times;
        }
        double avg_time = total_times / iterations;

        double avg_cycles = (double)total_cycles / iterations;
        double avg_nsec = avg_cycles *  g_config.cpu_period;
        double avg_sec = avg_nsec / 1e9;

        BenchResult current_res;

        current_res.elapsed_sec = avg_nsec / 1e9;    ////////////////

        // current_res.elapsed_sec = avg_time;

        current_res.valid_count = total_elements;
        current_res.bw_gbs = 0;
        current_res.usage = 0;
        current_res.latency_ns = 0;

        if (current_res.elapsed_sec > 0 && total_elements > 0) {
            double total_bytes = (double)total_elements * 64.0;


            // current_res.bw_gbs = (total_bytes / 1e9) / current_res.elapsed_sec;
            // current_res.latency_ns = (current_res.elapsed_sec * 1e9) / total_elements;

            current_res.latency_ns = avg_nsec / total_elements;
            current_res.bw_gbs = total_bytes  / avg_nsec;
            current_res.usage = current_res.bw_gbs/g_config.DRAM_BW*100;

        }

        if (g_config.verbose_output) {
            cout << t << "\t"
                 << fixed << setprecision(3) << current_res.bw_gbs
                 << fixed << setprecision(3) << "\t" << current_res.usage
                 << "\t" << current_res.latency_ns
                 << fixed << setprecision(3) << "\t" << (max_cycles - min_cycles)/avg_cycles
                 << "\t" << (double)total_elements * 64.0 / (min_cycles *  g_config.cpu_period)
                 << "\t" << (double)total_elements * 64.0 / (max_cycles *  g_config.cpu_period)
                 << "\t" << (double)total_elements * 64.0 / (min_cycles *  g_config.cpu_period) / g_config.DRAM_BW * 100
                 << endl;
        }

        if (current_res.bw_gbs > best_res.bw_gbs) {
            best_res = current_res;
            best_core = t;
            best_dev = (max_cycles - min_cycles)/avg_cycles;
        }
    }
    cout << " - best core: " << best_core << "\t with deviation " << best_dev << endl;

    return best_res;
}

