#pragma once
// SAPI-1 character set (AND-1Z character generator, KOI-8 cs2 for C0-FF)
// conversion to and from Unicode.
#include <stdint.h>
#include <string>

// UTF-8 representation of a SAPI screen code, used for text screen dumps.
// 00-1F frame graphics -> box drawing, 80-BF 2x3 semigraphics -> sextants,
// C0-FF Czech letters, undefined glyphs -> full block.
const char* sapi_code_to_utf8(uint8_t code);

// SAPI code for a Unicode code point typed on the host keyboard, or -1.
int sapi_code_from_unicode(uint32_t cp);

// Decode one UTF-8 code point from `s` at `pos` (advances pos); returns -1 on error.
int32_t utf8_next(const std::string& s, size_t& pos);

// Convert host text (UTF-8) to SAPI codes; unmappable characters are skipped.
std::string sapi_codes_from_utf8(const std::string& s);
