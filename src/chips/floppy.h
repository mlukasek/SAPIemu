#pragma once
// 8" floppy drive with a disk image, shared by the disk controllers (RPD-1, FDC-1).
//
// Disks: IBM 3740, single sided, single density (FM), 77 tracks x 26 sectors x 128 B.
// Images are in logical order (.img, 256256 bytes: track 0 sector 1 first). The
// controller works on a working copy in work/ unless the image is outside media/.
//
// Track layout of an IBM 3740 disk (bytes of 32 us at 360 rpm, 5208 per revolution),
// the same for every controller: gap 4a 40 x FF, 6 x 00, index mark FC, gap 1 26 x FF,
// then for each sector (188 bytes): 6 x 00, ID mark FE, C H R N, CRC (2), gap 2 11 x FF,
// 6 x 00, data mark FB, 128 data bytes, CRC (2), gap 3 27 x FF; gap 4b to the index.
#include <stdint.h>
#include <string>
#include <vector>

struct FloppyDrive {
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
    std::vector<uint8_t> data;    // IMAGE_SIZE when a disk is inserted
    bool write_protect = false;
    bool dirty = false;
    int track = 0;                // head position

    bool present(void) const { return !data.empty(); }
    // Read the image (through its working copy when work_copy); false + err if unusable.
    bool load(const std::string& image, bool work_copy, std::string& err);
    // Write a modified image back to its file.
    bool save(std::string& err);
    // Save and take the disk out.
    void unload(const char* who);
    // Sector 1..26 of a track, nullptr if outside the disk or no disk.
    uint8_t* sector(int trk, int r);
};

// CRC-CCITT as on IBM 3740 disks: start FFFF, x^16+x^12+x^5+1
uint16_t floppy_crc(uint16_t crc, uint8_t b);
