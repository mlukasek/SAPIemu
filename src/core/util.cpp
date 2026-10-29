#include "core/util.h"
#include "core/log.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <filesystem>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
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

bool file_write_safe(const std::string& utf8_path, const void* data, size_t size)
{
    fs::path dst = u8_to_path(utf8_path);
    fs::path tmp = dst;   // a name of its own: two processes saving the same image never share it
    tmp += ".tmp" + std::to_string((unsigned long long)std::random_device()());
    FILE* f = file_open(path_to_u8(tmp), "wb");
    if (!f) return file_write_all(utf8_path, data, size);   // no new file there (a long path...): as before
    bool ok = fwrite(data, 1, size, f) == size && fflush(f) == 0;
#ifdef _WIN32
    if (ok && _commit(_fileno(f)) != 0) ok = false;   // on the disk before the rename (a power cut)
#else
    if (ok && fsync(fileno(f)) != 0) ok = false;
#endif
    if (fclose(f) != 0) ok = false;
    std::error_code ec;
    if (!ok) {   // the disk full: the old image stays as it was
        fs::remove(tmp, ec);
        return false;
    }
    fs::rename(tmp, dst, ec);
    if (!ec) return true;
    fs::remove(tmp, ec);
    return file_write_all(utf8_path, data, size);
}

bool file_write_text(const std::string& utf8_path, const std::string& text)
{
#ifdef _WIN32
    std::string out;
    out.reserve(text.size() + text.size() / 16);
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) out += '\r';
        out += text[i];
    }
    return file_write_all(utf8_path, out.data(), out.size());
#else
    return file_write_all(utf8_path, text.data(), text.size());
#endif
}

static time_t fixed_time = 0;

time_t host_time_now(void)
{
    return fixed_time ? fixed_time : time(nullptr);
}

void host_time_fix(time_t t)
{
    fixed_time = t;
}

bool file_stat(const std::string& utf8_path, uint64_t& size, int64_t& mtime)
{
    std::error_code ec;
    fs::path p = u8_to_path(utf8_path);
    size = (uint64_t)fs::file_size(p, ec);
    if (ec) return false;
    auto t = fs::last_write_time(p, ec);
    if (ec) return false;
    mtime = (int64_t)t.time_since_epoch().count();
    return true;
}

bool file_exists(const std::string& utf8_path)
{
    std::error_code ec;
    return fs::exists(u8_to_path(utf8_path), ec);
}

bool dir_exists(const std::string& utf8_path)
{
    std::error_code ec;
    return fs::is_directory(u8_to_path(utf8_path), ec);
}

bool file_remove(const std::string& utf8_path)
{
    std::error_code ec;
    return fs::remove(u8_to_path(utf8_path), ec);
}

bool file_rename(const std::string& utf8_from, const std::string& utf8_to)
{
    std::error_code ec;
    fs::rename(u8_to_path(utf8_from), u8_to_path(utf8_to), ec);   // replaces `to` (MoveFileExW REPLACE_EXISTING, rename(2))
    return !ec;
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
static std::string g_work_root;   // empty: not decided yet (work_root)

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

void data_root_set(const std::string& dir)
{
    g_data_root = dir;
    g_work_root.clear();
}

const std::string& data_root(void)
{
    if (g_data_root.empty()) data_root_init();
    return g_data_root;
}

bool data_root_valid(void)
{
    return is_data_root(u8_to_path(data_root()));
}

static bool dir_writable(const fs::path& d)
{
    std::error_code ec;
    fs::create_directories(d, ec);
    if (!fs::is_directory(d, ec)) return false;
    fs::path probe = d / ("write-test-" + std::to_string((unsigned long long)std::random_device()()) + ".tmp");
    FILE* f = file_open(path_to_u8(probe), "wb");
    if (!f) return false;
    fclose(f);
    fs::remove(probe, ec);
    return true;
}

// SAPIemu/work in the per-user local application data
static fs::path user_work_dir(void)
{
#ifdef _WIN32
    const wchar_t* base = _wgetenv(L"LOCALAPPDATA");
    if (base && *base) return fs::path(base) / "SAPIemu" / "work";
#else
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return fs::path(xdg) / "SAPIemu" / "work";
    const char* home = getenv("HOME");
    if (home && *home) return fs::path(home) / ".local" / "share" / "SAPIemu" / "work";
#endif
    return fs::path();
}

const std::string& work_root(void)
{
    if (!g_work_root.empty()) return g_work_root;
    fs::path w = u8_to_path(data_root()) / "work";
    if (dir_writable(w)) {
        g_work_root = path_to_u8(w);
        return g_work_root;
    }
    fs::path u = user_work_dir();
    if (!u.empty() && dir_writable(u)) {
        g_work_root = path_to_u8(u);
        LOGI("the program folder is not writable, working files in %s", g_work_root.c_str());
    } else {
        g_work_root = path_to_u8(w);   // nothing writable: the errors come when a file is written
        LOGW("no writable folder for the working files (%s)", g_work_root.c_str());
    }
    return g_work_root;
}

// "work" or "work/..." (either separator): the rest after "work/", else false
static bool work_relative(const std::string& rel, std::string& rest)
{
    if (rel.compare(0, 4, "work") != 0) return false;
    if (rel.size() == 4) { rest.clear(); return true; }
    if (rel[4] != '/' && rel[4] != '\\') return false;
    rest = rel.substr(5);
    return true;
}

std::string data_path(const std::string& rel)
{
    fs::path p = u8_to_path(rel);
    if (p.is_absolute()) return rel;
    std::string rest;
    if (work_relative(rel, rest)) return rest.empty() ? work_root() : path_to_u8(u8_to_path(work_root()) / u8_to_path(rest));
    return path_to_u8(u8_to_path(data_root()) / p);
}

int file_lock_try(const std::string& utf8_path)
{
    // the handle stays open on purpose: the system closes it (and frees the lock) when the process ends, a crash too
#ifdef _WIN32
    std::wstring w = u8_to_path(utf8_path).wstring();
    HANDLE h = CreateFileW(w.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h != INVALID_HANDLE_VALUE) return 1;
    return GetLastError() == ERROR_SHARING_VIOLATION ? 0 : -1;   // else e.g. a folder without the right to write
#else
    int fd = open(utf8_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) return 1;
    close(fd);
    return 0;
#endif
}

static bool g_battery_files = true;

void battery_files_enable(bool on)
{
    g_battery_files = on;
}

bool battery_files_enabled(void)
{
    return g_battery_files;
}

static uint64_t g_media_writes = 0;   // the emulation thread only

void media_written(void)
{
    g_media_writes++;
}

uint64_t media_writes(void)
{
    return g_media_writes;
}

std::string data_relative(const std::string& path)
{
    std::error_code ec;
    fs::path p = fs::weakly_canonical(u8_to_path(path), ec);
    if (ec) return path;
    fs::path work = fs::weakly_canonical(u8_to_path(work_root()), ec);   // also outside the data root
    if (!ec) {
        fs::path rel = p.lexically_relative(work);
        std::string r = path_to_u8(rel);
        if (!rel.empty() && !rel.is_absolute() && r.rfind("..", 0) != 0) {
            for (char& c : r) if (c == '\\') c = '/';
            return r == "." ? "work" : "work/" + r;
        }
    }
    ec.clear();
    fs::path root = fs::weakly_canonical(u8_to_path(data_root()), ec);
    if (ec) return path;
    fs::path rel = p.lexically_relative(root);
    if (rel.empty() || rel.is_absolute()) return path;
    std::string r = path_to_u8(rel);
    if (r.rfind("..", 0) == 0) return path;
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

std::vector<std::string> list_machine_files(void)
{
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(u8_to_path(data_path("machines")), ec)) {
        std::filesystem::path ext = e.path().extension();
        if (ext != ".sapi" && ext != ".jsonc" && ext != ".json") continue;   // .jsonc/.json: older files
        if (path_to_u8(e.path().filename()).rfind("_", 0) == 0) continue;   // _vyber.json: the index, not a machine
        out.push_back(data_relative(path_to_u8(e.path())));
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> list_files(const std::string& rel_dir, const std::string& ext)
{
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(u8_to_path(data_path(rel_dir)), ec)) {
        if (!e.is_regular_file(ec)) continue;
        std::string x = path_to_u8(e.path().extension());
        if (x.size() != ext.size()) continue;
        bool same = true;
        for (size_t i = 0; i < x.size(); i++)
            if (tolower((unsigned char)x[i]) != tolower((unsigned char)ext[i])) same = false;
        if (same) out.push_back(data_relative(path_to_u8(e.path())));
    }
    std::sort(out.begin(), out.end());
    return out;
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
    fs::path dst = u8_to_path(work_root()) / u8_to_path(norm.substr(6));
    out = path_to_u8(dst);
    std::error_code ec;
    if (fs::exists(dst, ec)) {
        // a directory copy gets the files added to media/ since (the files already there are
        // the user's working copies and stay as they are)
        if (fs::is_directory(src, ec) && fs::is_directory(dst, ec)) {
            for (fs::recursive_directory_iterator it(src, ec), end; !ec && it != end; it.increment(ec)) {
                if (!it->is_regular_file(ec)) continue;
                fs::path to = dst / fs::relative(it->path(), src, ec);
                if (ec || fs::exists(to, ec)) continue;
                fs::create_directories(to.parent_path(), ec);
                if (fs::copy_file(it->path(), to, ec)) LOGI("working copy: added %s", path_to_u8(to).c_str());
                ec.clear();
            }
            ec.clear();
        }
        return true;
    }
    if (!fs::exists(src, ec)) {
        err = "media file not found: " + path_to_u8(src);
        return false;
    }
    fs::create_directories(dst.parent_path(), ec);
    // Copied under a temporary name and renamed: another emulator or test process creating the same
    // copy at the same time never sees a half written file, and the copy that came first is kept.
    fs::path tmp = dst;
    tmp += ".tmp" + std::to_string((unsigned long long)std::random_device()());
    bool dir = fs::is_directory(src, ec);
    if (dir) fs::copy(src, tmp, fs::copy_options::recursive, ec);   // e.g. the PMD32-SD "SD card"
    else fs::copy_file(src, tmp, ec);
    std::error_code ec2;
    if (!ec && !fs::exists(dst, ec2)) fs::rename(tmp, dst, ec);
    if (fs::exists(tmp, ec2)) fs::remove_all(tmp, ec2);   // not needed: created meanwhile, or failed
    if (fs::exists(dst, ec2)) return true;
    err = "cannot create working copy " + out + ": " + ec.message();
    return false;
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
