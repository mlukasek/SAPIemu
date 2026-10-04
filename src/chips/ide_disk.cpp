#include "chips/ide_disk.h"
#include "core/state.h"
#include "core/log.h"
#include "core/util.h"

#include <string.h>

#ifdef _WIN32
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#else
#define fseek64 fseeko
#define ftell64 ftello
#endif

IdeDisk::IdeDisk()
{
    memset(buffer, 0, sizeof(buffer));
}

IdeDisk::~IdeDisk()
{
    close();
}

bool IdeDisk::open(const std::string& p, bool ro, std::string& err)
{
    close();
    file = file_open(p, ro ? "rb" : "r+b");
    if (!file) {
        err = "cannot open IDE image " + p;
        return false;
    }
    fseek64(file, 0, SEEK_END);
    long long size = ftell64(file);
    sectors = (uint32_t)(size / 512);
    if (sectors == 0) {
        err = "IDE image is empty: " + p;
        close();
        return false;
    }
    image_path = p;
    readonly = ro;
    cyls = (int)(sectors / (uint32_t)(heads * spt));
    if (cyls > 16383) cyls = 16383;
    if (cyls < 1) cyls = 1;
    reset();
    return true;
}

void IdeDisk::close(void)
{
    if (file) fclose(file);
    file = nullptr;
}

void IdeDisk::flush(void)
{
    if (file) fflush(file);
}

void IdeDisk::reset(void)
{
    error = 0x01;   // diagnostic code: no error
    count = 1;
    lba0 = 1;
    lba1 = lba2 = 0;
    head = 0;
    status = ST_DRDY | ST_DSC;
    xfer = XFER_NONE;
    buf_pos = 0;
    remaining = 0;
}

bool IdeDisk::current_lba(uint32_t& lba) const
{
    if (head & 0x40) {
        lba = ((uint32_t)(head & 0x0F) << 24) | ((uint32_t)lba2 << 16) | ((uint32_t)lba1 << 8) | lba0;
    } else {
        // CHS: lba0 = sector (1-based), lba1/lba2 = cylinder, head = head
        uint32_t cyl = ((uint32_t)lba2 << 8) | lba1;
        uint32_t h = head & 0x0F;
        if (lba0 == 0) return false;
        lba = (cyl * (uint32_t)heads + h) * (uint32_t)spt + (uint32_t)(lba0 - 1);
    }
    return lba < sectors;
}

void IdeDisk::advance_address(void)
{
    uint32_t lba;
    if (!current_lba(lba)) return;
    lba++;
    if (head & 0x40) {
        lba0 = (uint8_t)lba; lba1 = (uint8_t)(lba >> 8); lba2 = (uint8_t)(lba >> 16);
        head = (uint8_t)((head & 0xF0) | ((lba >> 24) & 0x0F));
    } else {
        uint32_t s = lba % (uint32_t)spt, t = lba / (uint32_t)spt;
        lba0 = (uint8_t)(s + 1);
        head = (uint8_t)((head & 0xF0) | (t % (uint32_t)heads));
        uint32_t cyl = t / (uint32_t)heads;
        lba1 = (uint8_t)cyl; lba2 = (uint8_t)(cyl >> 8);
    }
}

bool IdeDisk::load_sector(uint32_t lba)
{
    if (fseek64(file, (long long)lba * 512, SEEK_SET) != 0) return false;
    return fread(buffer, 1, 512, file) == 512;
}

bool IdeDisk::store_sector(uint32_t lba)
{
    if (readonly) return true;   // writes are silently discarded on a read only image
    if (fseek64(file, (long long)lba * 512, SEEK_SET) != 0) return false;
    return fwrite(buffer, 1, 512, file) == 512;
}

static void put_string(uint8_t* buf, int word, int words, const char* s)
{
    // ATA strings: two characters per word, first character in the high byte
    size_t n = strlen(s);
    for (int i = 0; i < words * 2; i++) {
        char c = i < (int)n ? s[i] : ' ';
        buf[(word * 2) + (i ^ 1)] = (uint8_t)c;
    }
}

static void put_word(uint8_t* buf, int word, uint16_t v)
{
    buf[word * 2] = (uint8_t)v;
    buf[word * 2 + 1] = (uint8_t)(v >> 8);
}

void IdeDisk::identify(void)
{
    memset(buffer, 0, sizeof(buffer));
    put_word(buffer, 0, 0x0040);              // fixed disk
    put_word(buffer, 1, (uint16_t)cyls);
    put_word(buffer, 3, (uint16_t)heads);
    put_word(buffer, 6, (uint16_t)spt);
    put_string(buffer, 10, 10, "SAPIEMU0001");
    put_string(buffer, 23, 4, "1.0");
    put_string(buffer, 27, 20, "SAPIemu IDE disk");
    put_word(buffer, 47, 0x8001);
    put_word(buffer, 49, 0x0200);             // LBA supported
    put_word(buffer, 53, 0x0001);
    put_word(buffer, 54, (uint16_t)cyls);
    put_word(buffer, 55, (uint16_t)heads);
    put_word(buffer, 56, (uint16_t)spt);
    uint32_t chs = (uint32_t)cyls * heads * spt;
    put_word(buffer, 57, (uint16_t)chs);
    put_word(buffer, 58, (uint16_t)(chs >> 16));
    put_word(buffer, 60, (uint16_t)sectors);
    put_word(buffer, 61, (uint16_t)(sectors >> 16));
}

void IdeDisk::abort_cmd(uint8_t err)
{
    error = err;
    status = ST_DRDY | ST_DSC | ST_ERR;
    xfer = XFER_NONE;
}

void IdeDisk::command(uint8_t cmd)
{
    error = 0;
    xfer = XFER_NONE;
    switch (cmd) {
    case 0xEC:   // IDENTIFY DEVICE
        identify();
        buf_pos = 0;
        remaining = 1;
        xfer = XFER_READ;
        status = ST_DRDY | ST_DSC | ST_DRQ;
        break;
    case 0x20: case 0x21: {   // READ SECTORS
        uint32_t lba;
        remaining = count ? count : 256;
        if (!current_lba(lba)) { abort_cmd(ER_IDNF); break; }
        if (!load_sector(lba)) { abort_cmd(ER_UNC); break; }
        buf_pos = 0;
        xfer = XFER_READ;
        status = ST_DRDY | ST_DSC | ST_DRQ;
        break;
    }
    case 0x30: case 0x31: {   // WRITE SECTORS
        uint32_t lba;
        remaining = count ? count : 256;
        if (!current_lba(lba)) { abort_cmd(ER_IDNF); break; }
        buf_pos = 0;
        xfer = XFER_WRITE;
        status = ST_DRDY | ST_DSC | ST_DRQ;
        break;
    }
    case 0x10: case 0x11: case 0x12: case 0x13:   // RECALIBRATE
    case 0x91:                                    // INITIALIZE DEVICE PARAMETERS
    case 0xE0: case 0xE1: case 0xE2: case 0xE3:   // standby / idle
    case 0xE5:                                    // CHECK POWER MODE
    case 0xE7:                                    // FLUSH CACHE
    case 0xEF:                                    // SET FEATURES
    case 0xC6:                                    // SET MULTIPLE MODE
    case 0x40: case 0x41:                         // READ VERIFY
    case 0x70:                                    // SEEK
        if (cmd == 0xE5) count = 0xFF;            // active / idle
        if (cmd == 0xE7) flush();
        status = ST_DRDY | ST_DSC;
        break;
    case 0x90:   // EXECUTE DEVICE DIAGNOSTIC
        error = 0x01;
        status = ST_DRDY | ST_DSC;
        break;
    default:
        LOGD("IDE: unsupported command %02X", cmd);
        abort_cmd(ER_ABRT);
        break;
    }
}

uint16_t IdeDisk::read_reg(bool cs1, int reg)
{
    if (!file) return 0xFFFF;   // no drive: floating bus
    reg &= 7;
    if (!cs1) {
        if (reg == 6) return status;   // alternate status
        return 0xFF;                   // drive address register not used
    }
    if ((head & 0x10) && reg != 6) return 0x00;   // slave selected, not present
    switch (reg) {
    case 0: {
        if (xfer != XFER_READ) return 0xFFFF;
        uint16_t w = (uint16_t)(buffer[buf_pos] | (buffer[buf_pos + 1] << 8));
        buf_pos += 2;
        if (buf_pos >= 512) {
            buf_pos = 0;
            remaining--;
            advance_address();
            uint32_t lba;
            if (remaining > 0 && current_lba(lba) && load_sector(lba)) {
                status = ST_DRDY | ST_DSC | ST_DRQ;
            } else {
                xfer = XFER_NONE;
                status = ST_DRDY | ST_DSC;
            }
        }
        return w;
    }
    case 1: return error;
    case 2: return count;
    case 3: return lba0;
    case 4: return lba1;
    case 5: return lba2;
    case 6: return head;
    default: return status;
    }
}

void IdeDisk::write_reg(bool cs1, int reg, uint16_t value)
{
    if (!file) return;
    reg &= 7;
    uint8_t v = (uint8_t)value;
    if (!cs1) {
        if (reg == 6) {
            if ((v & 0x04) && !(devctl & 0x04)) reset();   // SRST
            devctl = v;
        }
        return;
    }
    switch (reg) {
    case 0:
        if (xfer != XFER_WRITE) return;
        buffer[buf_pos] = (uint8_t)value;
        buffer[buf_pos + 1] = (uint8_t)(value >> 8);
        buf_pos += 2;
        if (buf_pos >= 512) {
            uint32_t lba;
            buf_pos = 0;
            if (!current_lba(lba) || !store_sector(lba)) { abort_cmd(ER_IDNF); return; }
            remaining--;
            advance_address();
            if (remaining > 0) {
                status = ST_DRDY | ST_DSC | ST_DRQ;
            } else {
                xfer = XFER_NONE;
                status = ST_DRDY | ST_DSC;
            }
        }
        break;
    case 1: features = v; break;
    case 2: count = v; break;
    case 3: lba0 = v; break;
    case 4: lba1 = v; break;
    case 5: lba2 = v; break;
    case 6: head = v; break;
    default: command(v); break;
    }
}

void IdeDisk::state(StateIO& io)
{
    io.media("IDE disk", image_path);
    io.v(error);
    io.v(features);
    io.v(count);
    io.v(lba0);
    io.v(lba1);
    io.v(lba2);
    io.v(head);
    io.v(status);
    io.v(devctl);
    io.arr(buffer, sizeof(buffer));
    io.e(buf_pos);
    io.e(xfer);
    io.e(remaining);
}
