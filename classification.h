#pragma once

#include "types.h"

void sanitize_banks();
void verify_row_alignment();
void perform_classification(const vector<void*>& hugePagePtrs);
