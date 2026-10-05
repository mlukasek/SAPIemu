// ImageDisk images (chips/floppy.h): an 8" SS SD disk built from a plain image with interleave,
// compressed sectors, a deleted data mark and a data error loads with its sector IDs and flags,
// keeps its data in logical order (RPD-1) and saves back after a write; FORMAT rebuilds a track
// of an ImageDisk and is refused by a plain image of another format; the EC1045 system
// (machines/cpm_ec1045.sapi) saves a file on a 5.25" HD disk given as ImageDisk.
#include "chips/floppy.h"
#include "core/headless.h"
#include "core/machine.h"
#include "core/util.h"
#include "test_util.h"

#include <string.h>

using nlohmann::json;

// an ImageDisk of a plain image: physical order with the given skew, uniform sectors compressed
static std::vector<uint8_t> make_imd(const std::vector<uint8_t>& img, const FloppyGeom& g, int skew,
                                     int deleted_r, int crc_r)
{
    std::string head = "IMD 1.18: 05/10/2026 12:00:00\r\nSAPIemu test\r\n";
    std::vector<uint8_t> o(head.begin(), head.end());
    o.push_back(0x1A);
    int size = g.sector_size();
    uint8_t mode = g.mfm ? (g.kbps >= 500 ? 3 : 5) : (g.kbps >= 250 ? 0 : 2);
    for (int c = 0; c < g.tracks; c++)
        for (int h = 0; h < g.heads; h++) {
            o.push_back(mode);
            o.push_back((uint8_t)c);
            o.push_back((uint8_t)h);
            o.push_back((uint8_t)g.sectors);
            o.push_back((uint8_t)g.n);
            std::vector<int> rmap(g.sectors, 0);
            int pos = 0;
            for (int r = 1; r <= g.sectors; r++) {
                while (rmap[pos]) pos = (pos + 1) % g.sectors;
                rmap[pos] = r;
                pos = (pos + skew) % g.sectors;
            }
            for (int r : rmap) o.push_back((uint8_t)r);
            for (int r : rmap) {
                const uint8_t* p = &img[(((size_t)c * g.heads + h) * g.sectors + (size_t)(r - 1)) * size];
                bool same = true;
                for (int k = 1; k < size && same; k++) same = p[k] == p[0];
                int type = 1;
                if (c == 3 && h == 0 && r == deleted_r) type += 2;
                if (c == 3 && h == 0 && r == crc_r) type += 4;
                if (same) type += 1;
                o.push_back((uint8_t)type);
                if (same) o.push_back(p[0]);
                else o.insert(o.end(), p, p + size);
            }
        }
    return o;
}

static const FloppySector* find(FloppyTrack* t, int r)
{
    for (const FloppySector& s : t->sec)
        if (s.r == r) return &s;
    return nullptr;
}

static void imd_8inch(void)
{
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/rpd-1/CPM22P_48A_Perina.img"), img));
    const FloppyGeom* g = floppy_geom_for_size(img.size());
    CHECK(g != nullptr);
    if (!g) return;
    std::vector<uint8_t> imd = make_imd(img, *g, 6, 5, 7);
    CHECK(file_write_all(data_path("work/test/imd8.imd"), imd.data(), imd.size()));

    FloppyDrive d;
    std::string err;
    CHECK(d.load("work/test/imd8.imd", false, err));
    CHECK(d.imd);
    CHECK(d.is_8sssd());   // logical data for RPD-1
    CHECK(d.data.size() == img.size() && memcmp(d.data.data(), img.data(), img.size()) == 0);
    CHECK_EQ(d.cylinders, 77);
    FloppyTrack* t3 = d.track_at(3, 0);
    CHECK(t3 != nullptr && t3->sec.size() == 26 && !t3->mfm && t3->kbps == 250);
    if (t3) {
        CHECK_EQ((int)t3->sec[0].r, 1);
        CHECK_EQ((int)t3->sec[6].r, 2);   // skew 6
        const FloppySector* s5 = find(t3, 5);
        const FloppySector* s7 = find(t3, 7);
        CHECK(s5 && s5->flags == FS_DELETED);
        CHECK(s7 && s7->flags == FS_DATA_CRC);
    }
    CHECK(d.track_at(77, 0) == nullptr && d.track_at(0, 1) == nullptr);
    printf("%s\n", d.format.c_str());

    // a write, saved as ImageDisk, read back
    FloppyTrack* t10 = d.track_at(10, 0);
    const FloppySector* s = t10 ? find(t10, 9) : nullptr;
    CHECK(s != nullptr);
    if (s) {
        for (int k = 0; k < 128; k++) d.data[s->off + k] = (uint8_t)(k * 3 + 1);
        d.dirty = true;
    }
    CHECK(d.save(err));
    FloppyDrive e;
    CHECK(e.load("work/test/imd8.imd", false, err));
    FloppyTrack* u10 = e.track_at(10, 0);
    const FloppySector* s2 = u10 ? find(u10, 9) : nullptr;
    CHECK(s2 && e.data[s2->off + 5] == 16 && e.data[s2->off + 127] == (uint8_t)(127 * 3 + 1));
    FloppyTrack* u3 = e.track_at(3, 0);
    CHECK(u3 && find(u3, 5) && find(u3, 5)->flags == FS_DELETED);
    CHECK(e.data.size() == img.size());

    // FORMAT: an ImageDisk takes a new layout (track 20: MFM 8 x 512 here), a plain image refuses it
    std::vector<FloppySector> ids;
    for (int r = 1; r <= 8; r++) {
        FloppySector f;
        f.c = 20, f.h = 0, f.r = (uint8_t)r, f.n = 2;
        ids.push_back(f);
    }
    CHECK(e.format_track(20, 0, true, 500, 2, ids, 0xE5));
    FloppyTrack* u20 = e.track_at(20, 0);
    CHECK(u20 && u20->mfm && u20->size == 512 && u20->sec.size() == 8 && e.data[u20->sec[7].off + 511] == 0xE5);
    CHECK(!e.is_8sssd());
    CHECK(e.save(err));
    FloppyDrive f;
    CHECK(f.load("work/test/imd8.imd", false, err));
    FloppyTrack* v20 = f.track_at(20, 0);
    CHECK(v20 && v20->mfm && v20->sec.size() == 8 && v20->size == 512);
    FloppyDrive p;
    CHECK(p.load("media/rpd-1/CPM22P_48A_Perina.img", false, err));
    CHECK(!p.format_track(20, 0, true, 500, 2, ids, 0xE5));
}

// the EC1045 system with a 5.25" HD disk as ImageDisk: SAVE B: and the directory entry
static void imd_hd(void)
{
    std::vector<uint8_t> img;
    CHECK(file_read_all(data_path("media/fdc-1/prazdna_5.25HD_CPM.img"), img));
    const FloppyGeom* g = floppy_geom_for_size(img.size());
    CHECK(g != nullptr);
    if (!g) return;
    std::vector<uint8_t> imd = make_imd(img, *g, 1, 0, 0);
    CHECK(file_write_all(data_path("work/test/hd.imd"), imd.data(), imd.size()));
    std::string t;
    CHECK(file_read_text(data_path("machines/cpm_ec1045.sapi"), t));
    json cfg = json::parse(t, nullptr, false, true);
    for (json& c : cfg["cards"]) {
        std::string ty = c.value("type", std::string());
        if (ty == "ZRMD-1") {
            c["mode"] = "readonly";
            c["working_copy"] = false;
        }
        if (ty == "FDC-1") {
            c["drive_a"] = { { "image", "work/test/hd.imd" }, { "write_protect", false } };
            c["working_copy"] = false;
        }
    }
    Machine m;
    std::string err;
    CHECK(m.load_json(cfg, err));
    m.type_text("2");
    CHECK(run_until_text(m, "B,C: FDC1; 5,25\" DS,HD", 5.0));
    CHECK(run_until_text(m, "\nA>", 30.0));
    m.run_seconds(0.3);
    m.type_text("SAVE 8 B:TEST.COM\r");
    m.run_seconds(4.0);
    m.type_text("DIR B:\r");
    CHECK(run_until_text(m, "TEST    .COM", 10.0));
    m.flush();
    FloppyDrive d;
    CHECK(d.load("work/test/hd.imd", false, err));
    CHECK(d.imd && d.heads == 2 && d.cylinders == 80);
    FloppyTrack* t1 = d.track_at(1, 0);
    const FloppySector* s = t1 ? find(t1, 1) : nullptr;
    CHECK(s && memcmp(&d.data[s->off + 1], "TEST    COM", 11) == 0);
    printf("%s\n", screen_text(m).c_str());
}

int main(void)
{
    test_init();
    imd_8inch();
    imd_hd();
    return TEST_RESULT();
}
