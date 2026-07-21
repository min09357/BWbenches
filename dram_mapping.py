# dram_mapping.py — named DRAM address-mapping profiles (committed registry).
#
# Each entry is one machine's address-decode masks. To bring up a new machine,
# add a new named entry here, then set ADDR_PROFILE in config.py to its name.
# "0x0" means there is only one CH / DIMM slot / SubCH / RANK on that axis.

PROFILES = {
    "arrow_1r": dict(
        ch_func="0x0", slot_func="0x0", sch_func="0x82600", rank_func="0x0",
        bg_func="0x42102100,0x84204000,0x108408000",
        ba_func="0x210850000,0x210a0000",
        col_mask="0x1bc0", row_mask="0x3fffc0000",
    ),
    "arrow_2r": dict(
        ch_func="0x0", slot_func="0x0", sch_func="0x82600", rank_func="0x42120000",
        bg_func="0x84042100,0x108404000,0x210808000",
        ba_func="0x421090000,0x240000",
        col_mask="0x1bc0", row_mask="0x7fff80000",
    ),
    "cascade_2r": dict(
        ch_func="0x0", slot_func="0x0", sch_func="0x0", rank_func="0x2000",
        bg_func="0x40,0x40000",
        ba_func="0x80000,0x100000",
        col_mask="0x5f80", row_mask="0x7ffe38000",
    ),
    "spr_2r": dict(
        ch_func="0x0", slot_func="0x0", sch_func="0x40", rank_func="0x80",
        bg_func="0x10000100,0x20000200,0x100001400",
        ba_func="0x40000800,0x80001000",
        col_mask="0xfe000", row_mask="0xffff00000",
    ),
}
