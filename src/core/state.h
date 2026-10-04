#pragma once
// Save state of the machine (docs/stav.md). Every card, chip and keyboard has one function,
// state(StateIO&), that both writes its variables (saving) and reads them back in the same
// order (loading): io.v(x) for each variable. Blocks (begin/end) carry a name and a length,
// so a block that reads more or less than was written is reported instead of shifting the
// rest. Numbers are little endian.
#include <stdint.h>
#include <deque>
#include <string>
#include <vector>

class StateIO {
public:
    explicit StateIO(bool load) : loading(load) {}
    const bool loading;
    std::vector<uint8_t> data;
    size_t pos = 0;
    std::string error;           // the first problem; once set, nothing more is read
    bool ok(void) const { return error.empty(); }
    void fail(const std::string& why);

    void raw(void* p, size_t n);
    void v(uint8_t& x) { raw(&x, 1); }
    void v(int8_t& x) { raw(&x, 1); }
    void v(bool& x);
    void v(uint16_t& x);
    void v(int16_t& x);
    void v(uint32_t& x);
    void v(int32_t& x);
    void v(uint64_t& x);
    void v(int64_t& x);
    void v(float& x);
    void v(double& x);
    void v(std::string& s);
    void v(std::vector<uint8_t>& b);          // with its size
    void v(std::vector<int16_t>& b);
    void v(std::deque<uint8_t>& q);
    void v(std::vector<std::string>& l);
    void arr(uint8_t* p, size_t n) { raw(p, n); }
    // memory sized by the configuration: the size must match
    void mem(std::vector<uint8_t>& b);
    // enums and other ints as 32 bits
    template <class T> void e(T& x)
    {
        int32_t t = (int32_t)x;
        v(t);
        if (loading) x = (T)t;
    }
    // a size_t as 64 bits
    void sz(size_t& x);

    // named block: begin() writes / checks the name and the length, end() patches / checks it
    bool begin(const char* name);
    void end(void);
    // the name of the next block without reading it (loading)
    std::string peek_block(void);

    // a medium the state refers to (not stored): its file, size and time; on loading, a log
    // warning when the file changed since the state was saved
    void media(const char* what, const std::string& path);

private:
    std::vector<size_t> starts;  // open blocks: position of the length (save) / end (load)
};
