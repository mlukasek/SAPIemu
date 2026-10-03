#include "core/ihex.h"
#include "core/util.h"

#include <ctype.h>
#include <stdio.h>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_byte(const std::string& s, size_t pos)
{
    if (pos + 1 >= s.size()) return -1;
    int h = hex_nibble(s[pos]), l = hex_nibble(s[pos + 1]);
    return (h < 0 || l < 0) ? -1 : h * 16 + l;
}

bool ihex_parse(const std::string& text, ihex_byte_fn fn, void* user, std::string& err)
{
    uint32_t base = 0;
    int line_no = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find_first_of("\r\n", pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        line_no++;
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.empty()) continue;
        if (line[0] != ':') continue;   // tolerate comments / garbage lines
        int len = hex_byte(line, 1);
        int ah = hex_byte(line, 3), al = hex_byte(line, 5);
        int type = hex_byte(line, 7);
        if (len < 0 || ah < 0 || al < 0 || type < 0 || line.size() < (size_t)(11 + len * 2)) {
            err = "malformed record at line " + std::to_string(line_no);
            return false;
        }
        uint8_t sum = (uint8_t)(len + ah + al + type);
        uint8_t data[256];
        for (int i = 0; i < len; i++) {
            int b = hex_byte(line, 9 + i * 2);
            if (b < 0) { err = "bad data at line " + std::to_string(line_no); return false; }
            data[i] = (uint8_t)b;
            sum += (uint8_t)b;
        }
        int cks = hex_byte(line, 9 + len * 2);
        if (cks < 0 || (uint8_t)(sum + cks) != 0) {
            err = "checksum error at line " + std::to_string(line_no);
            return false;
        }
        uint32_t addr = (uint32_t)((ah << 8) | al);
        switch (type) {
        case 0x00:
            for (int i = 0; i < len; i++) fn(user, base + addr + i, data[i]);
            break;
        case 0x01:
            return true;
        case 0x02:
            if (len == 2) base = (uint32_t)((data[0] << 8) | data[1]) << 4;
            break;
        case 0x04:
            if (len == 2) base = (uint32_t)((data[0] << 8) | data[1]) << 16;
            break;
        default:
            break;
        }
    }
    return true;
}

struct RomLoadCtx {
    std::vector<uint8_t>* image;
    uint32_t base;
    int out_of_range;
};

static void rom_store(void* user, uint32_t addr, uint8_t value)
{
    RomLoadCtx* ctx = (RomLoadCtx*)user;
    uint32_t off = addr - ctx->base;
    if (addr < ctx->base || off >= ctx->image->size()) { ctx->out_of_range++; return; }
    (*ctx->image)[off] = value;
}

static bool looks_like_hex(const std::string& path, const std::vector<uint8_t>& data)
{
    size_t i = 0;
    while (i < data.size() && (data[i] == ' ' || data[i] == '\r' || data[i] == '\n' || data[i] == '\t')) i++;
    if (i >= data.size() || data[i] != ':') return false;
    std::string ext = path.size() >= 4 ? path.substr(path.size() - 4) : "";
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    return ext == ".hex" || ext == ".txt" || ext == ".ihx";
}

bool rom_load(const std::string& path, std::vector<uint8_t>& image, size_t size,
              uint8_t fill, uint32_t hex_base, std::string& err)
{
    std::string full = data_path(path);
    std::vector<uint8_t> data;
    if (!file_read_all(full, data)) {
        err = "cannot read " + full;
        return false;
    }
    image.assign(size, fill);
    if (looks_like_hex(full, data)) {
        RomLoadCtx ctx = { &image, hex_base, 0 };
        std::string text((const char*)data.data(), data.size());
        if (!ihex_parse(text, rom_store, &ctx, err)) {
            err = full + ": " + err;
            return false;
        }
        if (ctx.out_of_range) {
            err = full + ": " + std::to_string(ctx.out_of_range) + " bytes outside the ROM";
            return false;
        }
    } else {
        if (data.size() > size) {
            err = full + ": file is larger than the ROM (" + std::to_string(size) + " bytes)";
            return false;
        }
        for (size_t i = 0; i < data.size(); i++) image[i] = data[i];
    }
    return true;
}
