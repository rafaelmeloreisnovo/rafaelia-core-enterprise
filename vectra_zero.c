// ============================================================
// VECTRA ZERO-FRICTION
// Universal (ARM32 / ARM64 / x86)
// Sem libc pesada, sem GC, com medição física real quando possível
// ============================================================

#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>

#if defined(__aarch64__)
#define ARCH_ARM64 1
#else
#define ARCH_ARM64 0
#endif

// ============================================================
// TIMER (REAL OU FALLBACK)
// ============================================================
static inline uint64_t rdtime() {
#if ARCH_ARM64
    uint64_t v;
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + t.tv_nsec;
#endif
}

// ============================================================
// CRC (INVARIANTE)
// ============================================================
static inline uint32_t crc_kernel(uint8_t *p, size_t n) {
    uint32_t c = 0;

#if ARCH_ARM64
    // tenta usar hardware
    for (size_t i = 0; i + 8 <= n; i += 8) {
        uint64_t v = *(uint64_t*)(p + i);
        asm volatile("crc32x %w[c], %w[c], %x[v]"
            : [c] "+r"(c)
            : [v] "r"(v));
    }
    for (size_t i = (n & ~7); i < n; i++) {
        asm volatile("crc32b %w[c], %w[c], %w[v]"
            : [c] "+r"(c)
            : [v] "r"(p[i]));
    }
#else
    // fallback universal
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
    }
#endif

    return c;
}

// ============================================================
// MIX (VARIANTE – SEM NEON, MAS CACHE-FRIENDLY)
// ============================================================
static inline void mix_kernel(uint8_t *p, size_t n) {
    for (size_t i = 0; i + 64 <= n; i += 64) {
        uint8_t tmp[64];

        for (int j = 0; j < 64; j++)
            tmp[j] = p[i + j];

        for (int j = 0; j < 64; j++) {
            uint8_t a = tmp[(j + 7) & 63];
            uint8_t b = tmp[(j + 11) & 63];
            p[i + j] ^= a ^ b;
        }
    }
}

// ============================================================
// WRITE DIRETO (SEM STDIO)
// ============================================================
static void write_str(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    syscall(SYS_write, 1, s, n);
}

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

// ============================================================
// MAIN – PIPELINE UNIFICADO (56 CICLOS)
// ============================================================
int main() {

    static uint8_t buf[4096] __attribute__((aligned(64)));

    // seed determinístico (cache-aligned)
    uint64_t s = 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 4096; i += 8) {
        *(uint64_t*)(buf + i) = s;
        s += 0x9E3779B9;
    }

    uint64_t total = 0;
    uint32_t acc = 0;

    uint64_t min = (uint64_t)-1;
    uint64_t max = 0;

    // ========================================================
    // LOOP PRINCIPAL (56 ciclos)
    // ========================================================
    for (int i = 0; i < 56; i++) {

        uint64_t t0 = rdtime();
        uint32_t c = crc_kernel(buf, 4096);
        uint64_t t1 = rdtime();

        uint64_t d1 = t1 - t0;
        total += d1;

        if (d1 < min) min = d1;
        if (d1 > max) max = d1;

        acc ^= c;

        t0 = rdtime();
        mix_kernel(buf, 4096);
        t1 = rdtime();

        uint64_t d2 = t1 - t0;
        total += d2;

        if (d2 < min) min = d2;
        if (d2 > max) max = d2;
    }

    // ========================================================
    // OUTPUT (FÍSICO + LÓGICO)
    // ========================================================
    write_str("MODE:");
#if ARCH_ARM64
    write_str("ARM64\n");
#else
    write_str("GENERIC\n");
#endif

    write_str("CRC:");
    hex32(acc);

    write_str(" TOTAL:");
    hex64(total);

    write_str("\nMIN:");
    hex64(min);

    write_str(" MAX:");
    hex64(max);

    write_str("\n");

    return 0;
}
