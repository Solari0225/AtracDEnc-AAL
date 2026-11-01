#ifndef INDEPENDENT_RESTRICTED_PLUS_CORE_PARSER_H
#define INDEPENDENT_RESTRICTED_PLUS_CORE_PARSER_H
#include "../plus_core_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Independently read the actual encoded ATRAC3plus frame into the common
 * spectrum structure. Supports the music encoder's restricted syntax:
 * stereo, nonzero word lengths, mode-3/mode-1 word VLC, explicit scale/table
 * indices, table type A, sine windows, no swap/negate/gain/tones/noise, and
 * disabled power compensation. Other syntax is rejected explicitly.
 * Returns 1 on success and 0 on malformed or unsupported syntax.
 * It never reads encoder metadata or calls the encoder.
 */
int at3pcore_parse_spectrum(const uint8_t* frame,size_t frame_bytes,
                           at3pcore_spectrum* output);
const char* at3pcore_parse_last_error(void);
#ifdef __cplusplus
}
#endif
#endif
