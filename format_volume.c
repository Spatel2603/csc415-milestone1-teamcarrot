// format_volume.c
// CSC415 Milestone One – Write VCB, Free Space, and Root Directory

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

// =======================
// Configuration
// =======================
#define VOLUME_FILENAME "myvolume.dat"
#define BLOCK_SIZE      512u
#define TOTAL_BLOCKS    4096u

// =======================
// VCB Structure
// =======================
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         // "VCB1"
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t block_size;
    uint32_t total_blocks;

    uint32_t free_start;
    uint32_t free_len;

    uint32_t root_start;
    uint32_t root_len;

    uint8_t reserved[BLOCK_SIZE -
        (4 + 2 + 2 + 4 + 4 + 4 + 4 + 4 + 4)];
} VCB;
#pragma pack(pop)

// =======================
// Root Directory Entry
// =======================
#pragma pack(push, 1)
typedef struct {
    char name[32];
    uint32_t block_num;
    uint8_t type;   // 1 = directory, 2 = file
    uint8_t valid;
    uint8_t padding[BLOCK_SIZE - 32 - 4 - 1 - 1];
} RootDir;
#pragma pack(pop)

// =======================
// Helper Functions
// =======================
static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void write_block(FILE *f, uint32_t block_no, const void *data) {
    uint64_t offset = (uint64_t)block_no * BLOCK_SIZE;
    if (fseek(f, (long)offset, SEEK_SET) != 0) die("fseek");
    size_t written = fwrite(data, 1, BLOCK_SIZE, f);
    if (written != BLOCK_SIZE) die("fwrite");
    fflush(f);
}

static void ensure_size(FILE *f, uint64_t size_bytes) {
    if (fseek(f, (long)(size_bytes - 1), SEEK_SET) != 0) die("fseek end");
    if (fputc('\0', f) == EOF) die("fputc");
    fflush(f);
}

static uint32_t ceil_div(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

// =======================
// Layout Info
// =======================
typedef struct {
    uint32_t vcb_start;
    uint32_t vcb_len;
    uint32_t free_start;
    uint32_t free_len;
    uint32_t root_start;
    uint32_t root_len;
} Layout;

// =======================
// Compute Layout
// =======================
static Layout compute_layout(void) {
    Layout L;
    L.vcb_start  = 0;
    L.vcb_len    = 1;

    uint32_t bitmap_bytes = ceil_div(TOTAL_BLOCKS, 8);
    L.free_len   = ceil_div(bitmap_bytes, BLOCK_SIZE);
    L.free_start = L.vcb_start + L.vcb_len;

    L.root_start = L.free_start + L.free_len;
    L.root_len   = 1;

    return L;
}

// =======================
// Main
// =======================
int main(void) {
    printf("[Team] Formatting volume: %s\n", VOLUME_FILENAME);
    FILE *f = fopen(VOLUME_FILENAME, "wb+");
    if (!f) die("fopen");

    ensure_size(f, (uint64_t)TOTAL_BLOCKS * BLOCK_SIZE);

    Layout L = compute_layout();
    printf("[Team] Layout:\n");
    printf("  VCB: start=%u, len=%u\n", L.vcb_start, L.vcb_len);
    printf("  Free space: start=%u, len=%u\n", L.free_start, L.free_len);
    printf("  Root dir: start=%u, len=%u\n", L.root_start, L.root_len);

    // ---------------------
    // Block 0: Write VCB
    // ---------------------
    VCB vcb;
    memset(&vcb, 0, sizeof(vcb));
    vcb.magic         = 0x56434231; // "VCB1"
    vcb.version_major = 1;
    vcb.version_minor = 0;
    vcb.block_size    = BLOCK_SIZE;
    vcb.total_blocks  = TOTAL_BLOCKS;
    vcb.free_start    = L.free_start;
    vcb.free_len      = L.free_len;
    vcb.root_start    = L.root_start;
    vcb.root_len      = L.root_len;
    write_block(f, L.vcb_start, &vcb);
    printf("[Team] Wrote VCB to block 0.\n");

    // ---------------------
    // Block 1: Initialize Free Space Bitmap
    // ---------------------
    uint8_t bitmap[BLOCK_SIZE];
    memset(bitmap, 0x00, BLOCK_SIZE);

    // Mark blocks 0–2 (VCB, free, root) as used
    for (int i = 0; i < 3; i++) {
        bitmap[i / 8] |= (1 << (i % 8));
    }

    write_block(f, L.free_start, bitmap);
    printf("[Team] Wrote free-space bitmap to block 1.\n");

    // ---------------------
    // Block 2: Create Root Directory
    // ---------------------
    RootDir root;
    memset(&root, 0, sizeof(root));

    // Current directory "."
    strncpy(root.name, ".", sizeof(root.name));
    root.block_num = L.root_start;
    root.type = 1;
    root.valid = 1;
    write_block(f, L.root_start, &root);
    printf("[Team] Wrote root directory (.) entry to block 2.\n");

    fclose(f);
    printf("[Team] Volume formatted successfully.\n");
    printf("[Team] Dump block 0, 1, and 2 using HexDump to verify.\n");
    return 0;
}
