// PMD32-SD protocol, following pmd32.c of firmware v3.2 (Roman Bórik, 2012).
#include "chips/pmd32sd.h"
#include "core/state.h"
#include "chips/i8255.h"
#include "core/log.h"
#include "core/util.h"

#include <ctype.h>
#include <string.h>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static const uint8_t PRESENTATION = 0xAA, ACK = 0x33, NAK = 0x99;
static const uint32_t SEND_TIMEOUT = 30, SEND_PRESENT_TIMEOUT = 15, SEND_ACK_TIMEOUT = 500, SEND_NAK_TIMEOUT = 0;
static const uint32_t WAIT_PRESENT_TIMEOUT = 10, WAIT_COMMAND_TIMEOUT = 30, WAIT_BYTE_TIMEOUT = 5;
static const int SECTOR_SIZE = 128, PHYSICAL_SECTOR_SIZE = 512;
enum { FW_NO_TRANSFER = 0, FW_SEND_DATA = 1, FW_RECEIVE_DATA = 2, FW_FORMAT = 3, FW_ERROR = -1 };

static fs::path u8p(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }
static std::string p8(const fs::path& p) { std::u8string u = p.u8string(); return std::string(u.begin(), u.end()); }

static bool iequal(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

Pmd32Sd::Pmd32Sd()
{
    memset(buffer, 0, sizeof(buffer));
}

Pmd32Sd::~Pmd32Sd()
{
    for (int d = 0; d < DRIVES; d++) eject(d);
}

std::string Pmd32Sd::host_path(const std::string& rel) const
{
    return p8(u8p(sd_root) / u8p(rel));
}

// A path inside the SD card: '/' separated components, no "..", ".", empty
// components, drive letters or backslashes. Names come from guest software
// (commands H, N), which must not reach host files outside the SD card - the
// real device only sees its own FAT card.
bool Pmd32Sd::safe_path(const std::string& rel, bool allow_dirs)
{
    if (rel.empty() || rel.size() > 255) return false;
    size_t start = 0;
    for (;;) {
        size_t end = rel.find('/', start);
        std::string comp = rel.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (comp.empty() || comp == "." || comp == "..") return false;
        for (char c : comp)
            if ((unsigned char)c < 0x20 || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
                c == '<' || c == '>' || c == '|')
                return false;
        if (end == std::string::npos) return true;
        if (!allow_dirs) return false;
        start = end + 1;
    }
}

bool Pmd32Sd::init(const std::string& root, std::string& err)
{
    std::error_code ec;
    if (!fs::is_directory(u8p(root), ec)) {
        err = "PMD32-SD: SD card directory not found: " + root;
        return false;
    }
    sd_root = root;
    return true;
}

void Pmd32Sd::power_on(uint64_t t)
{
    now = t;
    pt = 0;
    cur_dir.clear();
    // drive A empty: insert the default system image from images.cfg
    if (!drives[0].f) {
        std::string def = default_image();
        if (!def.empty()) mount(0, def, false);
    }
}

void Pmd32Sd::flush(void)
{
    for (int d = 0; d < DRIVES; d++)
        if (drives[d].f) fflush(drives[d].f);
}

// ---------------------------------------------------------------- images.cfg

static std::vector<std::string> cfg_lines(const std::string& path)
{
    std::vector<std::string> lines;
    std::string text;
    if (!file_read_text(path, text)) return lines;
    std::string cur;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '\r' || c == '\n') {
            // CR, LF and CR LF each end one line (like the firmware's reader)
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') i++;
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

static std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

std::string Pmd32Sd::default_image(void) const
{
    for (const std::string& l : cfg_lines(host_path("images.cfg"))) {
        std::string t = trim(l);
        if (!t.empty() && t[0] == '*') return trim(t.substr(1));
    }
    return std::string();
}

int Pmd32Sd::check_image_type(const std::string& name, uint8_t& trks, uint8_t& secs, uint8_t& fsec) const
{
    size_t dot = name.rfind('.');
    if (dot == std::string::npos) return RESULT_UI;
    std::string ext = name.substr(dot + 1);
    std::string cfg = host_path("images.cfg");
    if (!file_exists(cfg)) return RESULT_CM;
    for (const std::string& l : cfg_lines(cfg)) {
        std::string t = trim(l);
        if (t.empty()) return RESULT_UI;   // the firmware stops at the first empty line
        if (t[0] == '*') continue;
        if (t.size() > 32) return RESULT_CC;
        char e[40];
        int a = -1, b = -1, c = 0;
        int n = sscanf(t.c_str(), "%39s %d %d %d", e, &a, &b, &c);
        if (n < 1 || !iequal(e, ext)) continue;
        if (n < 3 || a < 0 || a > 256 || b < 1 || b > 64 || c < 0 || c > 7) return RESULT_CC;
        trks = (uint8_t)a;   // 256 is stored as 0
        secs = (uint8_t)b;
        fsec = (uint8_t)c;
        return RESULT_OK;
    }
    return RESULT_UI;
}

std::vector<std::string> Pmd32Sd::list_images(void) const
{
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(u8p(sd_root), ec)) {
        if (!e.is_regular_file()) continue;
        std::string n = p8(e.path().filename());
        uint8_t t, s, f;
        if (check_image_type(n, t, s, f) == RESULT_OK) out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}

int Pmd32Sd::mount(int drive, const std::string& rel, bool wp)
{
    if (drive < 0 || drive >= DRIVES) return RESULT_BD;
    eject(drive);
    if (!safe_path(rel, true)) return RESULT_NF;   // outside the SD card
    uint8_t t, s, f;
    int rv = check_image_type(rel, t, s, f);
    if (rv != RESULT_OK) return rv;
    FILE* fp = file_open(host_path(rel), "r+b");
    if (!fp) {
        fp = file_open(host_path(rel), "rb");   // read only file
        if (fp) wp = true;
    }
    if (!fp) return RESULT_NF;
    drives[drive].f = fp;
    drives[drive].path = rel;
    drives[drive].wp = wp;
    drives[drive].tracks = t;
    drives[drive].sectors = s;
    drives[drive].phys = f;
    return RESULT_OK;
}

void Pmd32Sd::eject(int drive)
{
    if (drive < 0 || drive >= DRIVES) return;
    if (drives[drive].f) fclose(drives[drive].f);
    drives[drive].f = nullptr;
    drives[drive].path.clear();
}

// ---------------------------------------------------------------- sectors

bool Pmd32Sd::prepare_sector(void)
{
    Drive& d = drives[drvnum];
    if (!d.f || sector >= d.sectors || (d.tracks != 0 && track >= d.tracks)) return false;
    long seek = (long)track * d.sectors * SECTOR_SIZE + (long)sector * SECTOR_SIZE;
    if (fseek(d.f, seek, SEEK_SET) != 0) return false;
    return fread(buffer, 1, SECTOR_SIZE, d.f) == (size_t)SECTOR_SIZE;
}

bool Pmd32Sd::write_sector(void)
{
    Drive& d = drives[drvnum];
    long size;
    if (command == 'S') {
        sector &= 0x3C;
        size = PHYSICAL_SECTOR_SIZE;
    } else if (command == 'T' || command == 'W') {
        size = SECTOR_SIZE;
    } else {   // 'F'
        size = (long)d.sectors * SECTOR_SIZE;
        sector = 0;
        memset(buffer, 0xE5, sizeof(buffer));
    }
    if (!d.f || sector >= d.sectors || (d.tracks != 0 && track >= d.tracks)) return false;
    long seek = (long)track * d.sectors * SECTOR_SIZE + (long)sector * SECTOR_SIZE;
    if (fseek(d.f, seek, SEEK_SET) != 0) return false;
    while (size > 0) {
        long n = size > PHYSICAL_SECTOR_SIZE ? PHYSICAL_SECTOR_SIZE : size;
        if (fwrite(buffer, 1, (size_t)n, d.f) != (size_t)n) return false;
        size -= n;
    }
    fflush(d.f);
    return true;
}

int Pmd32Sd::create_image(const std::string& name)
{
    // the firmware creates the file in the current directory: a plain file name
    if (!safe_path(name, false)) return RESULT_CE;
    uint8_t t, s, f;
    int rv = check_image_type(name, t, s, f);
    if (rv != RESULT_OK) return rv;
    long size = (long)s * SECTOR_SIZE * (t == 0 ? 256 : t);
    std::string rel = cur_dir.empty() ? name : cur_dir + "/" + name;
    if (file_exists(host_path(rel))) return RESULT_CE;   // FAT create fails on an existing name
    std::vector<uint8_t> data((size_t)size, 0xE5);
    return file_write_all(host_path(rel), data.data(), data.size()) ? RESULT_OK : RESULT_CE;
}

void Pmd32Sd::build_listing(void)
{
    listing.clear();
    listing_pos = 0;
    std::error_code ec;
    std::vector<std::string> dirs, files;
    for (auto& e : fs::directory_iterator(u8p(host_path(cur_dir)), ec)) {
        std::string n = p8(e.path().filename());
        if (n.empty() || n[0] == '.') continue;   // hidden
        if (e.is_directory()) dirs.push_back("[" + n + "]");
        else files.push_back(n);
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());
    if (!cur_dir.empty()) listing.push_back("[..]");
    listing.insert(listing.end(), dirs.begin(), dirs.end());
    listing.insert(listing.end(), files.begin(), files.end());
}

// ---------------------------------------------------------------- primitives

void Pmd32Sd::start_op(OpKind kind, uint8_t value, uint32_t ms)
{
    op_kind = kind;
    op_value = value;
    op_ms = ms;
    op_stage = 0;
    op_ok = false;
    op_deadline = now + ms_to_cycles(ms);
    if (kind == OP_SEND) ppi->strobe_a(value);
}

bool Pmd32Sd::op_step(void)
{
    switch (op_kind) {
    case OP_DELAY:
        if (now < op_deadline) return false;
        op_ok = true;
        return true;
    case OP_SEND:
        if (op_ms == 0 || !ppi->a_input_full()) { op_ok = true; return true; }
        if (now >= op_deadline) { op_ok = false; return true; }
        return false;
    case OP_READ:
    case OP_READ_EX:
        if (ppi->a_output_full()) {
            op_byte = ppi->ack_a();
            if (op_kind == OP_READ_EX) crc ^= op_byte;
            op_ok = true;
            return true;
        }
        if (now < op_deadline) return false;
        if (op_kind == OP_READ_EX) ppi->strobe_a(NAK);   // send_byte(NAK, 0)
        op_ok = false;
        return true;
    case OP_CRC_CHECK:
        if (op_stage == 0) {
            if (ppi->a_output_full()) {
                op_byte = ppi->ack_a();
                crc ^= op_byte;
                if (crc == 0) {
                    ppi->strobe_a(ACK);
                    op_stage = 1;
                    op_deadline = now + ms_to_cycles(SEND_ACK_TIMEOUT);
                    return false;
                }
                ppi->strobe_a(NAK);
                op_ok = false;
                return true;
            }
            if (now < op_deadline) return false;
            ppi->strobe_a(NAK);
            op_ok = false;
            return true;
        }
        if (!ppi->a_input_full()) { op_ok = true; return true; }
        if (now >= op_deadline) {
            ppi->strobe_a(NAK);
            op_ok = false;
            return true;
        }
        return false;
    }
    return true;
}

// Resumable main loop. PT_WAIT returns from run() until the operation finishes.
#define PT_BEGIN() switch (pt) { case 0:
#define PT_END() } pt = 0;
#define DO(kind, value, ms) do { start_op(kind, value, ms); pt = __LINE__; [[fallthrough]]; case __LINE__: if (!op_step()) return; } while (0)

void Pmd32Sd::advance(uint64_t t, uint32_t hz)
{
    now = t;
    cpu_hz = hz;
    if (ppi) run();
}

void Pmd32Sd::run(void)
{
    PT_BEGIN();
    DO(OP_DELAY, 0, startup_ms);
    for (;;) {
        DO(OP_SEND, PRESENTATION, SEND_PRESENT_TIMEOUT);
        if (!op_ok) continue;
        DO(OP_READ, 0, WAIT_PRESENT_TIMEOUT);
        if (!op_ok || op_byte != PRESENTATION) continue;

        for (;;) {
            DO(OP_READ, 0, WAIT_COMMAND_TIMEOUT);
            if (!op_ok) break;
            command = op_byte;
            if (command == PRESENTATION) continue;
            crc = command;
            op = FW_NO_TRANSFER;

            if (command == 'B' || command == '*' || command == '@' || command == 'K') {
                DO(OP_CRC_CHECK, 0, WAIT_BYTE_TIMEOUT);
                if (op_ok) {
                    if (command == 'B') {
                        drvnum = 0; track = 0; sector = 0;
                        op = FW_SEND_DATA;
                        length = SECTOR_SIZE;
                    } else if (command == 'K') {
                        op = FW_SEND_DATA;
                        length = (uint16_t)cur_dir.size();
                    }
                } else {
                    op = FW_ERROR;
                }
            } else if (command == 'Q' || command == 'R' || command == 'F' ||
                       command == 'T' || command == 'W' || command == 'S') {
                // read_sector_track(): drive+sector, track
                DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                if (!op_ok) { op = FW_ERROR; goto after_parse; }
                drvnum = (uint8_t)(op_byte >> 6);
                if (drvnum == 1 || drvnum == 2) drvnum ^= 3;
                sector = op_byte & 0x3F;
                DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                if (!op_ok) { op = FW_ERROR; goto after_parse; }
                track = op_byte;
                if ((command == 'Q' || command == 'R') && drvnum < DRIVES && cpm_drive && *cpm_drive >= 0 &&
                    *cpm_drive < 16)
                    cpm_seen[drvnum] = (char)('A' + *cpm_drive);
                if (command == 'Q' || command == 'R' || command == 'F') {
                    DO(OP_CRC_CHECK, 0, WAIT_BYTE_TIMEOUT);
                    if (!op_ok) { op = FW_ERROR; goto after_parse; }
                    if (command == 'F') op = FW_FORMAT;
                    else { op = FW_SEND_DATA; length = SECTOR_SIZE; }
                } else {
                    op = FW_RECEIVE_DATA;
                    length = (command == 'S') ? PHYSICAL_SECTOR_SIZE + 1 : SECTOR_SIZE;
                }
            } else if (command == 'I' || command == 'G' || command == 'L' || command == 'P') {
                op = FW_ERROR;
                DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                if (op_ok) {
                    val = op_byte;
                    if (command == 'L') { if (val == 0) build_listing(); }
                    else drvnum = val;
                    DO(OP_CRC_CHECK, 0, WAIT_BYTE_TIMEOUT);
                    if (op_ok) op = (command != 'I') ? FW_SEND_DATA : FW_NO_TRANSFER;
                }
            } else if (command == 'H' || command == 'M' || command == 'N') {
                op = FW_ERROR;
                if (command == 'H') {
                    DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                    if (!op_ok) goto after_parse;
                    drvnum = op_byte;
                    DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                    if (!op_ok) goto after_parse;
                    sector = op_byte;   // write protect flag
                }
                DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                if (op_ok) { length = op_byte; op = FW_RECEIVE_DATA; }
            } else if (command == 'U' || command == 'C' || command == 'J') {
                // PMD 32 RAM access - not implemented in the firmware either
                op = FW_ERROR;
                DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                if (!op_ok) goto after_parse;
                DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                if (!op_ok) goto after_parse;
                if (command == 'J') {
                    DO(OP_CRC_CHECK, 0, WAIT_BYTE_TIMEOUT);
                    if (op_ok) op = FW_NO_TRANSFER;
                } else {
                    DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                    if (!op_ok) goto after_parse;
                    length = (uint16_t)(op_byte << 8);
                    DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                    if (!op_ok) goto after_parse;
                    length |= op_byte;
                    if (command == 'C') {
                        DO(OP_CRC_CHECK, 0, WAIT_BYTE_TIMEOUT);
                        if (op_ok) op = FW_SEND_DATA;
                    } else {
                        op = FW_RECEIVE_DATA;
                    }
                }
            } else {
                op = FW_ERROR;   // unknown command: no answer, back to presentation
            }
        after_parse:

            if (op == FW_NO_TRANSFER) {
                if (command == 'I' && drvnum >= DRIVES) {
                    DO(OP_SEND, RESULT_BD, SEND_TIMEOUT);
                    op = FW_ERROR;
                } else {
                    DO(OP_SEND, RESULT_OK, SEND_TIMEOUT);
                    if (!op_ok) op = FW_ERROR;
                }
            } else if (op == FW_SEND_DATA) {
                if (command == 'G') {
                    if (drvnum >= DRIVES) {
                        DO(OP_SEND, RESULT_BD, SEND_TIMEOUT);
                        op = FW_ERROR;
                    } else {
                        memset(buffer, 0, sizeof(buffer));
                        buffer[1] = drives[drvnum].wp ? 1 : 0;
                        length = (uint16_t)std::min<size_t>(drives[drvnum].path.size(), 63);
                        memcpy(buffer + 3, drives[drvnum].path.data(), length);
                        buffer[2] = (uint8_t)length;
                        length += 3;
                    }
                } else if (command == 'K') {
                    buffer[0] = (uint8_t)length;
                    memcpy(buffer + 1, cur_dir.data(), length);
                    length++;
                } else if (command == 'L') {
                    buffer[0] = 0;
                    length = 1;
                    if (listing_pos < listing.size()) {
                        const std::string& n = listing[listing_pos++];
                        size_t l = std::min<size_t>(n.size(), sizeof(buffer) - 2);
                        memcpy(buffer + 1, n.data(), l);
                        buffer[0] = (uint8_t)l;
                        length = (uint16_t)(l + 1);
                    }
                } else if (command == 'P') {
                    if (drvnum >= DRIVES) {
                        DO(OP_SEND, RESULT_BD, SEND_TIMEOUT);
                        op = FW_ERROR;
                    } else {
                        DO(OP_SEND, RESULT_OK, SEND_TIMEOUT);
                        if (!op_ok) {
                            op = FW_ERROR;
                        } else {
                            Drive& d = drives[drvnum];
                            buffer[0] = d.f ? d.tracks : 0;
                            buffer[1] = d.f ? d.sectors : 0;
                            buffer[2] = d.f ? d.phys : 0;
                            length = 3;
                        }
                    }
                } else if (command != 'C') {
                    if (!prepare_sector()) {
                        DO(OP_SEND, RESULT_RE, SEND_TIMEOUT);
                        op = FW_ERROR;
                    } else {
                        DO(OP_SEND, RESULT_OK, SEND_TIMEOUT);
                        if (!op_ok) op = FW_ERROR;
                    }
                }

                if (op != FW_ERROR) {
                    crc = 0;
                    for (idx = 0; idx < length; idx++) {
                        val = (command == 'C') ? 0xAA : buffer[idx];
                        crc ^= val;
                        DO(OP_SEND, val, SEND_TIMEOUT);
                        if (!op_ok) { op = FW_ERROR; break; }
                    }
                    if (op != FW_ERROR) {
                        DO(OP_SEND, crc, SEND_TIMEOUT);
                        if (!op_ok) op = FW_ERROR;
                    }
                }
            } else if (op == FW_RECEIVE_DATA || op == FW_FORMAT) {
                idx = 0;
                if (op == FW_RECEIVE_DATA) {
                    if (command == 'H' && length == 0xFF) {
                        buffer[idx++] = 0xFF;
                        length = 0;
                    }
                    for (temp = length; temp > 0; temp--) {
                        DO(OP_READ_EX, 0, WAIT_BYTE_TIMEOUT);
                        if (!op_ok) { op = FW_ERROR; break; }
                        if (command != 'U' && idx < sizeof(buffer) - 1) buffer[idx++] = op_byte;
                    }
                    if (op != FW_ERROR) {
                        DO(OP_CRC_CHECK, 0, WAIT_BYTE_TIMEOUT);
                        if (!op_ok) op = FW_ERROR;
                    }
                }

                if (op != FW_ERROR) {
                    if (command == 'M') {
                        buffer[idx] = 0;
                        val = RESULT_NF;
                        {
                            std::string name((const char*)buffer);
                            if (name == "..") {
                                size_t sl = cur_dir.rfind('/');
                                if (sl == std::string::npos) cur_dir.clear(); else cur_dir.erase(sl);
                                val = RESULT_OK;
                            } else if (name != ".") {
                                std::error_code ec;
                                for (auto& e : fs::directory_iterator(u8p(host_path(cur_dir)), ec)) {
                                    if (!e.is_directory()) continue;
                                    std::string n = p8(e.path().filename());
                                    if (!iequal(n, name)) continue;
                                    if (cur_dir.size() + 1 + n.size() >= 64) { val = RESULT_TL; break; }
                                    cur_dir = cur_dir.empty() ? n : cur_dir + "/" + n;
                                    val = RESULT_OK;
                                    break;
                                }
                            } else {
                                val = RESULT_OK;
                            }
                        }
                    } else if (command == 'H' || command == 'N') {
                        buffer[idx] = 0;
                        if (command == 'H' && drvnum >= DRIVES) {
                            val = RESULT_BD;
                        } else if (command == 'H' && buffer[0] == 0xFF) {
                            drives[drvnum].wp = sector != 0;
                            val = RESULT_OK;
                        } else if (length == 0) {
                            if (command == 'H') { eject(drvnum); val = RESULT_OK; }
                            else val = RESULT_CE;
                        } else if (cur_dir.size() + 1 + length >= 64) {
                            val = RESULT_TL;
                        } else if (command == 'H') {
                            std::string name((const char*)buffer);
                            val = (uint8_t)mount(drvnum, cur_dir.empty() ? name : cur_dir + "/" + name, sector != 0);
                        } else {
                            val = (uint8_t)create_image(std::string((const char*)buffer));
                        }
                    } else if (command != 'U' && (drives[drvnum < DRIVES ? drvnum : 0].wp || card_locked)) {
                        val = RESULT_WP;
                    } else if (command == 'U' || write_sector()) {
                        val = RESULT_OK;
                        if (command == 'F') {
                            DO(OP_DELAY, 0, 200);
                        }
                    } else {
                        val = (command == 'F') ? RESULT_FE : RESULT_WE;
                    }
                    DO(OP_SEND, val, SEND_TIMEOUT);
                    if (!op_ok) op = FW_ERROR;
                }
            }

            if (op == FW_ERROR) break;
        }
    }
    PT_END();
}

void Pmd32Sd::state(StateIO& io)
{
    for (int d = 0; d < DRIVES; d++) {
        std::string p = drives[d].path;
        bool wp = drives[d].wp;
        io.v(p);
        io.v(wp);
        io.media("PMD32-SD image", p.empty() ? std::string() : host_path(p));
        if (io.loading && io.ok()) {
            if (p != drives[d].path) {
                if (p.empty()) eject(d);
                else if (mount(d, p, wp) != RESULT_OK) LOGW("state: PMD32-SD %c: %s cannot be inserted", 'A' + d, p.c_str());
            }
            drives[d].wp = wp;
        }
    }
    io.v(card_locked);
    io.v(now);
    io.e(pt);
    io.e(op_kind);
    io.e(op_stage);
    io.v(op_value);
    io.v(op_ms);
    io.v(op_deadline);
    io.v(op_ok);
    io.v(op_byte);
    io.v(command);
    io.v(crc);
    io.v(val);
    io.e(op);
    io.v(drvnum);
    io.v(sector);
    io.v(track);
    io.v(length);
    io.v(temp);
    io.v(idx);
    io.arr(buffer, sizeof(buffer));
    io.v(cur_dir);
    io.v(listing);
    io.sz(listing_pos);
}
