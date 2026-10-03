#include "core/util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static fs::path u8_to_path(const std::string& s)
{
    return fs::path(std::u8string(s.begin(), s.end()));
}

static std::string path_to_u8(const fs::path& p)
{
    std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

FILE* file_open(const std::string& utf8_path, const char* mode)
{
#ifdef _WIN32
    std::wstring wpath = u8_to_path(utf8_path).wstring();
    wchar_t wmode[8] = {0};
    for (int i = 0; mode[i] && i < 7; i++) wmode[i] = (wchar_t)mode[i];
    return _wfopen(wpath.c_str(), wmode);
#else
    return fopen(utf8_path.c_str(), mode);
#endif
}

bool file_read_all(const std::string& utf8_path, std::vector<uint8_t>& out)
{
    FILE* f = file_open(utf8_path, "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}

bool file_read_text(const std::string& utf8_path, std::string& out)
{
    std::vector<uint8_t> data;
    if (!file_read_all(utf8_path, data)) return false;
    out.assign((const char*)data.data(), data.size());
    return true;
}

bool file_write_all(const std::string& utf8_path, const void* data, size_t size)
{
    FILE* f = file_open(utf8_path, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    if (fclose(f) != 0) ok = false;
    return ok;
}

bool file_exists(const std::string& utf8_path)
{
    std::error_code ec;
    return fs::exists(u8_to_path(utf8_path), ec);
}

bool make_dirs(const std::string& utf8_dir)
{
    std::error_code ec;
    fs::create_directories(u8_to_path(utf8_dir), ec);
    return fs::is_directory(u8_to_path(utf8_dir), ec);
}

std::string path_join(const std::string& a, const std::string& b)
{
    return path_to_u8(u8_to_path(a) / u8_to_path(b));
}

std::string path_dir(const std::string& p)
{
    return path_to_u8(u8_to_path(p).parent_path());
}

static std::string g_data_root;

static bool is_data_root(const fs::path& d)
{
    std::error_code ec;
    return fs::is_directory(d / "roms", ec) && fs::is_directory(d / "machines", ec);
}

static fs::path exe_dir(void)
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(NULL, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return fs::current_path();
    return fs::path(std::wstring(buf, n)).parent_path();
#else
    std::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() : p.parent_path();
#endif
}

void data_root_init(void)
{
    if (!g_data_root.empty()) return;
    std::error_code ec;
    for (fs::path d = exe_dir(); !d.empty(); d = d.parent_path()) {
        if (is_data_root(d)) { g_data_root = path_to_u8(d); return; }
        if (d == d.parent_path()) break;
    }
    for (fs::path d = fs::current_path(ec); !d.empty(); d = d.parent_path()) {
        if (is_data_root(d)) { g_data_root = path_to_u8(d); return; }
        if (d == d.parent_path()) break;
    }
#ifdef SAPIEMU_SOURCE_DIR
    if (is_data_root(u8_to_path(SAPIEMU_SOURCE_DIR))) { g_data_root = SAPIEMU_SOURCE_DIR; return; }
#endif
    g_data_root = ".";
}

void data_root_set(const std::string& dir) { g_data_root = dir; }

const std::string& data_root(void)
{
    if (g_data_root.empty()) data_root_init();
    return g_data_root;
}

std::string data_path(const std::string& rel)
{
    fs::path p = u8_to_path(rel);
    if (p.is_absolute()) return rel;
    return path_to_u8(u8_to_path(data_root()) / p);
}

bool media_path(const std::string& rel, bool work_copy, std::string& out, std::string& err)
{
    std::string norm = rel;
    for (char& c : norm) if (c == '\\') c = '/';
    if (!work_copy || norm.rfind("media/", 0) != 0) {
        out = data_path(rel);
        return true;
    }
    fs::path src = u8_to_path(data_path(norm));
    fs::path dst = u8_to_path(data_root()) / "work" / u8_to_path(norm.substr(6));
    out = path_to_u8(dst);
    std::error_code ec;
    if (fs::exists(dst, ec)) return true;
    if (!fs::exists(src, ec)) {
        err = "media file not found: " + path_to_u8(src);
        return false;
    }
    fs::create_directories(dst.parent_path(), ec);
    if (!fs::copy_file(src, dst, ec)) {
        err = "cannot create working copy " + out + ": " + ec.message();
        return false;
    }
    return true;
}

bool parse_number(const std::string& text, long long& out)
{
    std::string s;
    for (char c : text) if (!isspace((unsigned char)c)) s += c;
    if (s.empty()) return false;
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s = s.substr(2); base = 16; }
    else if (s[0] == '$') { s = s.substr(1); base = 16; }
    else if (s.back() == 'h' || s.back() == 'H') { s.pop_back(); base = 16; }
    if (s.empty()) return false;
    char* end = NULL;
    long long v = strtoll(s.c_str(), &end, base);
    if (*end != 0) return false;
    out = v;
    return true;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string unescape_text(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c != '\\' || i + 1 >= s.size()) { out += c; continue; }
        char e = s[++i];
        switch (e) {
        case 'r': out += '\r'; break;
        case 'n': out += '\r'; break;   // SAPI uses CR as line end
        case 't': out += '\t'; break;
        case 'e': out += '\x1b'; break;
        case '\\': out += '\\'; break;
        case 'x':
            if (i + 2 < s.size() && hexval(s[i + 1]) >= 0 && hexval(s[i + 2]) >= 0) {
                out += (char)(hexval(s[i + 1]) * 16 + hexval(s[i + 2]));
                i += 2;
            } else {
                out += "\\x";
            }
            break;
        default: out += '\\'; out += e; break;
        }
    }
    return out;
}
