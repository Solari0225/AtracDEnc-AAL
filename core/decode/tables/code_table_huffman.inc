/* Indexed code-table Huffman constants and direct widths only. */
static const plus_code_huff code_table_short[4] = {
    {0,1},
    {2,2},
    {6,3},
    {7,3},
};
static const plus_code_huff code_table_full_absolute[8] = {
    {0,2},
    {2,3},
    {3,3},
    {4,3},
    {5,3},
    {6,3},
    {14,4},
    {15,4},
};
static const plus_code_huff code_table_full_delta[8] = {
    {0,2},
    {2,3},
    {3,3},
    {4,3},
    {14,4},
    {15,4},
    {5,3},
    {6,3},
};
static const plus_code_huff code_table_full_stereo_delta[8] = {
    {0,1},
    {4,3},
    {10,4},
    {11,4},
    {12,4},
    {13,4},
    {14,4},
    {15,4},
};
static const uint8_t code_table_direct_widths[2] = {2,3};
