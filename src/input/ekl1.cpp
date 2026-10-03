#include "input/ekl1.h"

void Ekl1::type_char(uint32_t cp)
{
    bool ok;
    uint8_t c = koi8cs_keyboard_code(cp, ok);
    if (ok) push_code(c);
}

bool Ekl1::host_key(int key, int mods)
{
    int c = wordstar_code(key, mods);
    if (c < 0) return false;
    push_code((uint8_t)c);
    return true;
}
