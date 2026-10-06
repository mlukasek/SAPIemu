#pragma once
// Floppy drive with a disk image, shared by the disk controllers (RPD-1, FDC-1, MFC-1A).
//
// Image files:
//  - plain sector dumps in logical order (cylinder 0 head 0 sector 1 first, then head 1 of the
//    same cylinder); the format is given by the size (FloppyGeom):
//      256256  8" SS SD, IBM 3740: 77 x 1 x 26 x 128 B, FM 250 kbit/s, 360 rpm (all controllers)
//     1474560  3.5" DS HD (PC 1.44 MB): 80 x 2 x 18 x 512 B, MFM 500 kbit/s, 300 rpm
//     1228800  5.25" DS HD (PC 1.2 MB): 80 x 2 x 15 x 512 B, MFM 500 kbit/s, 360 rpm
//      737280  3.5"/5.25" DS DD (720 KB): 80 x 2 x 9 x 512 B, MFM 250 kbit/s, 300 rpm
//      368640  5.25" DS DD (360 KB): 40 x 2 x 9 x 512 B, MFM 250 kbit/s, 300 rpm
//  - ImageDisk (.imd, by its "IMD" header): every track with its own recording (FM/MFM), data
//    rate, sector size and sector IDs (any numbering, interleave, C and H), deleted data marks,
//    data CRC errors and sectors without data. Writes and FORMAT are saved back as ImageDisk.
// The controller works on a working copy in work/ unless the image is outside media/.
//
// The disk is a set of tracks (FloppyTrack): the sectors in their order from the index with
// the IDs and their data. Track layouts in disk bytes (32 us FM 250 kbit/s, 16 us MFM 500
// kbit/s, 32 us MFM 250 kbit/s, 64 us FM 125 kbit/s), gap 3 as long as the track allows:
//  FM, IBM 3740: gap 4a 40 x FF, 6 x 00, index mark FC, gap 1 26 x FF, then for each sector:
//   6 x 00, ID mark FE, C H R N, CRC (2), gap 2 11 x FF, 6 x 00, data mark FB, data, CRC (2),
//   gap 3 (27 x FF for 128 B sectors); gap 4b to the index.
//  MFM, IBM System 34: gap 4a 80 x 4E, 12 x 00, C2 C2 C2 FC, gap 1 50 x 4E, then for each sector:
//   12 x 00, A1 A1 A1 FE, C H R N, CRC (2), gap 2 22 x 4E, 12 x 00, A1 A1 A1 FB, data, CRC (2),
//   gap 3; gap 4b to the index.
#include <stdint.h>
#include <string>
#include <vector>

// plain image formats
struct FloppyGeom {
    const char* name;       // "8\" SS SD", "3.5\" DS HD" ...
    int size;               // image bytes
    int tracks, heads, sectors;
    int n;                  // sector size code: 128 << n bytes
    bool mfm;
    int kbps;               // data rate the disk was written with
    int rpm;

    int sector_size(void) const { return 128 << n; }
};

// The format of a plain image of `size` bytes, nullptr if none fits.
const FloppyGeom* floppy_geom_for_size(size_t size);
// All plain formats (the first is the 8" SS SD disk), ends with nullptr.
const FloppyGeom* const* floppy_geoms(void);

enum { FS_DELETED = 1, FS_DATA_CRC = 2, FS_NO_DATA = 4 };   // FloppySector::flags

struct FloppySector {
    uint8_t c = 0, h = 0, r = 0, n = 0;   // ID field
    uint8_t flags = 0;                    // FS_*
    uint32_t off = 0;                     // offset of the data in FloppyDrive::data
};

struct FloppyTrack {
    bool formatted = false;
    bool mfm = false;
    int kbps = 250;
    int size = 128;                       // data bytes of each sector (128 << n)
    uint8_t imd_mode = 0;                 // ImageDisk mode byte (0-5)
    std::vector<FloppySector> sec;        // in their order from the index
    // layout in disk bytes
    int byte_us = 32;                     // time of a byte (us)
    int track_len = 5208;                 // bytes per revolution
    int first_sector = 73;                // the first sector (its sync) after the index
    int sector_len = 188;                 // a sector with its gaps
    int id_mark = 6;                      // ID mark in a sector, C H R N follow
    int data_mark = 30;                   // data mark in a sector, the data follow
};

struct FloppyDrive {
    // the 8" SS SD disk of RPD-1
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
    std::vector<uint8_t> data;    // sector data (a plain image: the image itself)
    std::vector<FloppyTrack> tracks;   // cylinder * 2 + head
    const FloppyGeom* geom;       // plain format, or an ImageDisk that is one (logical data); nullptr else
    std::string format;           // description shown in the Media window
    bool imd = false;             // the file is ImageDisk
    std::string imd_header;       // its header text (up to the 1Ah)
    int cylinders = 0, heads = 1;
    int rpm = 360;
    bool write_protect = false;
    bool dirty = false;
    int track = 0;                // head position
    char cpm_cfg = 0;             // CP/M letter of the drive from the machine description (0 = not given)
    char cpm_seen = 0;            // CP/M letter seen while running (Bus::cpm_drive at a sector read)
    void seen_as(int cpm_drive) { if (cpm_drive >= 0 && cpm_drive < 16) cpm_seen = (char)('A' + cpm_drive); }

    // Mechanical stop of the head: an 80-track drive reaches a few tracks more (an assumption, the
    // same for all drives). Tracks formatted beyond a plain image stay in memory until the disk is
    // taken out; ImageDisk keeps them.
    static const int LAST_TRACK = 82;

    FloppyDrive();
    bool present(void) const { return !tracks.empty(); }
    bool is_8sssd(void) const;    // RPD-1: an 8" SS SD disk with its data in logical order
    // Read the image (through its working copy when work_copy); false + err if unusable.
    bool load(const std::string& image, bool work_copy, std::string& err);
    // Write a modified image back to its file.
    bool save(std::string& err);
    // A new disk formatted as g (every byte E5h, an empty CP/M directory): ImageDisk when the name
    // ends with .imd (any format can be written on it later), else a plain image.
    static bool create(const std::string& path, const FloppyGeom* g, std::string& err);
    // Save and take the disk out.
    void unload(const char* who);
    // Track of a cylinder and head, nullptr when there is none (unformatted, outside the disk).
    FloppyTrack* track_at(int cyl, int head);
    // FORMAT A TRACK: the new sectors (IDs), all filled with `fill`; false when the image cannot
    // keep them (a plain image in another format).
    bool format_track(int cyl, int head, bool mfm, int kbps, int n, const std::vector<FloppySector>& ids,
                      uint8_t fill);

    // Layout of a track (gaps, positions) for its recording, rate, sector size and count and the rpm.
    void layout(FloppyTrack& t) const;

private:
    bool load_plain(std::vector<uint8_t>& d, std::string& err);
    bool load_imd(const std::vector<uint8_t>& d, std::string& err);
    bool save_imd(std::string& err);
    void clear(void);
};

// CRC-CCITT as on IBM 3740 disks: start FFFF, x^16+x^12+x^5+1
uint16_t floppy_crc(uint16_t crc, uint8_t b);
