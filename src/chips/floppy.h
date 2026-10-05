#pragma once
// Floppy drive with a disk image, shared by the disk controllers (RPD-1, FDC-1, MFC-1A).
//
// Images are plain sector dumps in logical order (track 0 head 0 sector 1 first, then head 1
// of the same cylinder); the format is given by the size (FloppyGeom):
//   256256  8" SS SD, IBM 3740: 77 x 1 x 26 x 128 B, FM 250 kbit/s, 360 rpm (all controllers)
//  1474560  3.5" DS HD (PC 1.44 MB): 80 x 2 x 18 x 512 B, MFM 500 kbit/s, 300 rpm
//  1228800  5.25" DS HD (PC 1.2 MB): 80 x 2 x 15 x 512 B, MFM 500 kbit/s, 360 rpm
//   737280  3.5"/5.25" DS DD (720 KB): 80 x 2 x 9 x 512 B, MFM 250 kbit/s, 300 rpm
//   368640  5.25" DS DD (360 KB): 40 x 2 x 9 x 512 B, MFM 250 kbit/s, 300 rpm
// The controller works on a working copy in work/ unless the image is outside media/.
//
// Track layouts in disk bytes (32 us FM 250 kbit/s, 16 us MFM 500 kbit/s, 32 us MFM 250 kbit/s):
//  FM, IBM 3740: gap 4a 40 x FF, 6 x 00, index mark FC, gap 1 26 x FF, then for each sector
//   (188 bytes): 6 x 00, ID mark FE, C H R N, CRC (2), gap 2 11 x FF, 6 x 00, data mark FB,
//   128 data bytes, CRC (2), gap 3 27 x FF; gap 4b to the index.
//  MFM, IBM System 34: gap 4a 80 x 4E, 12 x 00, C2 C2 C2 FC, gap 1 50 x 4E, then for each sector:
//   12 x 00, A1 A1 A1 FE, C H R N, CRC (2), gap 2 22 x 4E, 12 x 00, A1 A1 A1 FB, data, CRC (2),
//   gap 3; gap 4b to the index.
#include <stdint.h>
#include <string>
#include <vector>

struct FloppyGeom {
    const char* name;       // "8\" SS SD", "3.5\" DS HD" ...
    int size;               // image bytes
    int tracks, heads, sectors;
    int n;                  // sector size code: 128 << n bytes
    bool mfm;
    int kbps;               // data rate the disk was written with
    int rpm;
    // layout in disk bytes
    int byte_us;            // time of a byte (us)
    int track_len;          // bytes per revolution
    int first_sector;       // offset of the first sector (its sync) after the index
    int sector_len;         // a sector with its gaps
    int id_mark;            // offset of the ID mark (FE) in a sector, C H R N follow
    int data_mark;          // offset of the data mark (FB) in a sector, the data follow

    int sector_size(void) const { return 128 << n; }
};

// The format of an image of `size` bytes, nullptr if none fits.
const FloppyGeom* floppy_geom_for_size(size_t size);
// All known formats (the first is the 8" SS SD disk), ends with nullptr.
const FloppyGeom* const* floppy_geoms(void);

struct FloppyDrive {
    // the 8" SS SD disk of RPD-1 (and the default before a disk is inserted)
    static const int TRACKS = 77;
    static const int SECTORS = 26;
    static const int SECTOR_SIZE = 128;
    static const int IMAGE_SIZE = TRACKS * SECTORS * SECTOR_SIZE;
    static const int TRACK_LEN = 5208;          // bytes per revolution (166.7 ms at 32 us)
    static const int FIRST_SECTOR = 73;         // first sector (its 6 zeros) after the index
    static const int SECTOR_LEN = 188;          // bytes per sector with its gaps
    static const int ID_MARK = 6;               // offset of the ID mark FE in a sector
    static const int DATA_MARK = 30;            // offset of the data mark FB in a sector

    std::string image;            // as configured (relative to the data root)
    std::string path;             // file in use (working copy)
    std::vector<uint8_t> data;    // geom->size when a disk is inserted
    const FloppyGeom* geom;       // format of the inserted disk (8" SS SD when none)
    bool write_protect = false;
    bool dirty = false;
    int track = 0;                // head position

    FloppyDrive();
    bool present(void) const { return !data.empty(); }
    bool is_8sssd(void) const { return geom == floppy_geoms()[0]; }
    // Read the image (through its working copy when work_copy); false + err if unusable.
    bool load(const std::string& image, bool work_copy, std::string& err);
    // Write a modified image back to its file.
    bool save(std::string& err);
    // Save and take the disk out.
    void unload(const char* who);
    // Sector r (from 1) of a track and head, nullptr if outside the disk or no disk.
    uint8_t* sector(int trk, int head, int r);
};

// CRC-CCITT as on IBM 3740 disks: start FFFF, x^16+x^12+x^5+1
uint16_t floppy_crc(uint16_t crc, uint8_t b);
