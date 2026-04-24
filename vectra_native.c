#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>

// ============================================
// TIMER DIRETO (sem libc)
// ============================================
static inline uint64_t rdcycle() {
#if defined(__aarch64__)
    uint64_t v;
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
#else
    return 0;
#endif
}

// ============================================
// DETECÇÃO REAL (AUXV)
// ============================================
#if defined(__aarch64__)
#include <sys/auxv.h>
#define HWCAP_CRC32 (1 << 7)

static int has_crc() {
    unsigned long hw = getauxval(AT_HWCAP);
    return (hw & HWCAP_CRC32) != 0;
}
#else
static int has_crc() { return 0; }
#endif

// ============================================
// CRC HARDWARE
// ============================================
static uint32_t crc_hw(uint8_t *p, size_t n) {
    uint32_t c = 0;
#if defined(__aarch64__)
    for (size_t i = 0; i + 8 <= n; i += 8) {
        uint64_t v = *(uint64_t*)(p + i);
        asm volatile("crc32x %w[c], %w[c], %x[v]"
            : [c] "+r"(c)
            : [v] "r"(v));
    }
#endif
    return c;
}

// ============================================
// CRC SOFTWARE
// ============================================
static uint32_t crc_sw(uint8_t *p, size_t n) {
    uint32_t c = 0;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
    }
    return c;
}

// ============================================
// VARIANTE (SEM NEON – UNIVERSAL)
// ============================================
static void mix(uint8_t *p, size_t n) {
    for (size_t i = 0; i + 64 <= n; i += 64) {
        for (int j = 0; j < 64; j++) {
            uint8_t a = p[i + ((j + 7) & 63)];
            uint8_t b = p[i + ((j + 11) & 63)];
            p[i + j] ^= a ^ b;
        }
    }
}

// ============================================
// WRITE DIRETO
// ============================================
static void write_str(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    syscall(SYS_write, 1, s, n);
}

// ============================================
// HEX
// ============================================
static void hex32(uint32_t v) {
    char h[] = "0123456789abcdef";
    char out[8];
    for (int i = 7; i >= 0; i--) {
        out[i] = h[v & 0xF];
        v >>= 4;
    }
    syscall(SYS_write, 1, out, 8);
}

static void hex64(uint64_t v) {
    char h[] = "0123456789abcdef";
    char out[16];
    for (int i = 15; i >= 0; i--) {
        out[i] = h[v & 0xF];
        v >>= 4;
    }
    syscall(SYS_write, 1, out, 16);
}

// ============================================
// MAIN
// ============================================
int main() {

    static uint8_t buf[4096] __attribute__((aligned(64)));

    // init determinístico
    uint64_t s = 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 4096; i += 8) {
        *(uint64_t*)(buf + i) = s;
        s += 0x9E3779B9;
    }

    uint64_t total = 0;
    uint32_t acc = 0;

    int hw = has_crc();

    for (int i = 0; i < 56; i++) {

        uint64_t t0 = rdcycle();

        uint32_t c = hw ? crc_hw(buf, 4096)
                        : crc_sw(buf, 4096);

        uint64_t t1 = rdcycle();
        total += (t1 - t0);

        acc ^= c;

        t0 = rdcycle();
        mix(buf, 4096);
        t1 = rdcycle();
        total += (t1 - t0);
    }

    write_str("MODE:");
    write_str(hw ? "HW\n" : "SW\n");

    write_str("CRC:");
    hex32(acc);
    write_str(" CYC:");
    hex64(total);
    write_str("\n");

    return 0;
}
