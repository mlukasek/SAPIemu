#include "core/png.h"
#include "core/util.h"

static uint32_t crc_table[256];
static bool crc_init = false;

static uint32_t crc32_update(uint32_t crc, const uint8_t* p, size_t n)
{
    if (!crc_init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_table[i] = c;
        }
        crc_init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static void put32(std::vector<uint8_t>& v, uint32_t x)
{
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x);
}

static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data)
{
    put32(out, (uint32_t)data.size());
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put32(out, crc32_update(0, &out[start], out.size() - start));
}

std::vector<uint8_t> png_encode_argb(const uint32_t* argb, int w, int h, int sx, int sy)
{
    if (sx < 1) sx = 1;
    if (sy < 1) sy = 1;
    int W = w * sx, H = h * sy;

    // Two-colour images (text screens) are stored as 1-bit palette images, which
    // keeps them small without a real deflate compressor.
    uint32_t c0 = (w * h > 0) ? argb[0] : 0, c1 = c0;
    bool two_colors = true;
    for (int i = 0; i < w * h && two_colors; i++) {
        uint32_t p = argb[i];
        if (p == c0) continue;
        if (c1 == c0) c1 = p;
        else if (p != c1) two_colors = false;
    }

    // raw scanlines with filter byte 0
    std::vector<uint8_t> raw;
    if (two_colors) {
        int stride = (W + 7) / 8;
        raw.reserve((size_t)(stride + 1) * H);
        for (int y = 0; y < H; y++) {
            raw.push_back(0);
            const uint32_t* row = argb + (y / sy) * w;
            for (int bx = 0; bx < stride; bx++) {
                uint8_t byte = 0;
                for (int k = 0; k < 8; k++) {
                    int x = bx * 8 + k;
                    if (x < W && row[x / sx] != c0) byte |= (uint8_t)(0x80 >> k);
                }
                raw.push_back(byte);
            }
        }
    } else {
        raw.reserve((size_t)(W * 3 + 1) * H);
        for (int y = 0; y < H; y++) {
            raw.push_back(0);
            const uint32_t* row = argb + (y / sy) * w;
            for (int x = 0; x < W; x++) {
                uint32_t p = row[x / sx];
                raw.push_back((uint8_t)(p >> 16));
                raw.push_back((uint8_t)(p >> 8));
                raw.push_back((uint8_t)p);
            }
        }
    }

    // zlib stream with stored (uncompressed) deflate blocks
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    while (pos < raw.size() || raw.empty()) {
        size_t n = raw.size() - pos;
        if (n > 65535) n = 65535;
        bool last = pos + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back((uint8_t)n); z.push_back((uint8_t)(n >> 8));
        z.push_back((uint8_t)~n); z.push_back((uint8_t)(~n >> 8));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
        if (last) break;
    }
    put32(z, (b << 16) | a);

    std::vector<uint8_t> out = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    std::vector<uint8_t> ihdr;
    put32(ihdr, (uint32_t)W);
    put32(ihdr, (uint32_t)H);
    ihdr.push_back(two_colors ? 1 : 8);   // bit depth
    ihdr.push_back(two_colors ? 3 : 2);   // palette / RGB
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);
    if (two_colors) {
        std::vector<uint8_t> plte = {
            (uint8_t)(c0 >> 16), (uint8_t)(c0 >> 8), (uint8_t)c0,
            (uint8_t)(c1 >> 16), (uint8_t)(c1 >> 8), (uint8_t)c1,
        };
        chunk(out, "PLTE", plte);
    }
    chunk(out, "IDAT", z);
    chunk(out, "IEND", std::vector<uint8_t>());
    return out;
}

bool png_write_argb(const std::string& path, const uint32_t* argb, int w, int h, int sx, int sy)
{
    std::vector<uint8_t> png = png_encode_argb(argb, w, h, sx, sy);
    return file_write_all(path, png.data(), png.size());
}
