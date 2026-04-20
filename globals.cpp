#include "types.h"

Config g_config;

// [Optimized] 전역 메인 저장소: RowData 객체 대신 포인터를 저장
// addr[CH][SLOT][SC][RK][BG][BANK]
vector<RowData*> addr[MAX_CH][MAX_SLOT][MAX_SC][MAX_RK][MAX_BG][MAX_BANK];


// 전역 난수 생성기
std::mt19937 g_gen;
