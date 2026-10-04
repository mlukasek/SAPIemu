#include "core/state.h"
#include "core/log.h"
#include "core/util.h"

#include <string.h>

void StateIO::fail(const std::string& why)
{
    if (error.empty()) error = why;
}

void StateIO::raw(void* p, size_t n)
{
    if (!ok()) return;
    if (loading) {
        if (pos + n > data.size()) {
            fail("the state ends too early");
            memset(p, 0, n);
            return;
        }
        memcpy(p, data.data() + pos, n);
    } else {
        data.insert(data.end(), (const uint8_t*)p, (const uint8_t*)p + n);
    }
    pos += n;
}

template <class T> static void le(StateIO& io, T& x)
{
    uint8_t b[sizeof(T)];
    if (!io.loading) {
        uint64_t u = (uint64_t)x;
        for (size_t i = 0; i < sizeof(T); i++) b[i] = (uint8_t)(u >> (8 * i));
    }
    io.raw(b, sizeof(T));
    if (io.loading) {
        uint64_t u = 0;
        for (size_t i = 0; i < sizeof(T); i++) u |= (uint64_t)b[i] << (8 * i);
        x = (T)u;
    }
}

void StateIO::v(bool& x)
{
    uint8_t b = x ? 1 : 0;
    raw(&b, 1);
    if (loading) x = b != 0;
}
void StateIO::v(uint16_t& x) { le(*this, x); }
void StateIO::v(int16_t& x) { le(*this, x); }
void StateIO::v(uint32_t& x) { le(*this, x); }
void StateIO::v(int32_t& x) { le(*this, x); }
void StateIO::v(uint64_t& x) { le(*this, x); }
void StateIO::v(int64_t& x) { le(*this, x); }

void StateIO::v(float& x)
{
    uint32_t u;
    memcpy(&u, &x, 4);
    v(u);
    if (loading) memcpy(&x, &u, 4);
}

void StateIO::v(double& x)
{
    uint64_t u;
    memcpy(&u, &x, 8);
    v(u);
    if (loading) memcpy(&x, &u, 8);
}

void StateIO::sz(size_t& x)
{
    uint64_t u = x;
    v(u);
    if (loading) x = (size_t)u;
}

void StateIO::v(std::string& s)
{
    uint32_t n = (uint32_t)s.size();
    v(n);
    if (loading) {
        if (!ok() || pos + n > data.size()) { fail("the state ends too early"); return; }
        s.assign((const char*)data.data() + pos, n);
        pos += n;
    } else {
        raw(&s[0], n);
    }
}

void StateIO::v(std::vector<uint8_t>& b)
{
    uint32_t n = (uint32_t)b.size();
    v(n);
    if (loading) {
        if (!ok() || pos + n > data.size()) { fail("the state ends too early"); return; }
        b.assign(data.begin() + (ptrdiff_t)pos, data.begin() + (ptrdiff_t)(pos + n));
        pos += n;
    } else if (n) {
        raw(b.data(), n);
    }
}

void StateIO::v(std::vector<int16_t>& b)
{
    uint32_t n = (uint32_t)b.size();
    v(n);
    if (loading) {
        if (!ok() || pos + (size_t)n * 2 > data.size()) { fail("the state ends too early"); return; }
        b.resize(n);
    }
    for (int16_t& s : b) v(s);
}

void StateIO::v(std::deque<uint8_t>& q)
{
    uint32_t n = (uint32_t)q.size();
    v(n);
    if (loading) {
        q.clear();
        for (uint32_t i = 0; i < n && ok(); i++) {
            uint8_t b = 0;
            v(b);
            q.push_back(b);
        }
    } else {
        for (uint8_t b : q) v(b);
    }
}

void StateIO::v(std::vector<std::string>& l)
{
    uint32_t n = (uint32_t)l.size();
    v(n);
    if (loading) l.assign(ok() ? n : 0, std::string());
    for (std::string& s : l) v(s);
}

void StateIO::mem(std::vector<uint8_t>& b)
{
    uint32_t n = (uint32_t)b.size();
    v(n);
    if (loading && ok() && n != b.size()) {
        fail("memory of another size in the state (" + std::to_string(n) + " B, the card has " +
             std::to_string(b.size()) + " B)");
        return;
    }
    if (n) raw(b.data(), n);
}

bool StateIO::begin(const char* name)
{
    std::string nm = name;
    if (!loading) {
        v(nm);
        starts.push_back(data.size());
        uint32_t len = 0;
        v(len);
        return true;
    }
    std::string got;
    v(got);
    uint32_t len = 0;
    v(len);
    if (!ok()) return false;
    if (got != nm) {
        fail("block '" + got + "' where '" + nm + "' was expected");
        return false;
    }
    starts.push_back(pos + len);
    return true;
}

void StateIO::end(void)
{
    if (starts.empty()) return;
    size_t at = starts.back();
    starts.pop_back();
    if (!loading) {
        uint32_t len = (uint32_t)(data.size() - at - 4);
        for (int i = 0; i < 4; i++) data[at + (size_t)i] = (uint8_t)(len >> (8 * i));
        return;
    }
    if (ok() && pos != at) fail("a block of the state has another length (another version of the emulator?)");
    pos = at;
}

std::string StateIO::peek_block(void)
{
    size_t keep = pos;
    std::string err = error;
    std::string nm;
    v(nm);
    pos = keep;
    error = err;
    return nm;
}

void StateIO::media(const char* what, const std::string& path)
{
    std::string p = path;
    uint64_t size = 0;
    int64_t mtime = 0;
    if (!loading && !p.empty()) file_stat(p, size, mtime);
    v(p);
    v(size);
    v(mtime);
    if (!loading || !ok() || p.empty()) return;
    uint64_t s2 = 0;
    int64_t t2 = 0;
    if (!file_stat(p, s2, t2)) LOGW("state: %s %s is missing", what, p.c_str());
    else if (s2 != size || t2 != mtime)
        LOGW("state: %s %s changed since the state was saved (it is not part of the state)", what, p.c_str());
}
