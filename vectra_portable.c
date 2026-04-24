#include <stdio.h>
#include <stdint.h>
#include <time.h>

// =====================================================
// DETECÇÃO SIMPLES DE ARQUITETURA
// =====================================================
#if defined(__aarch64__)
#define ARCH_ARM64 1
#else
#define ARCH_ARM64 0
#endif

#if defined(__arm__)
#define ARCH_ARM32 1
#else
#define ARCH_ARM32 0
#endif

#if defined(__x86_64__)
#define ARCH_X86_64 1
#else
#define ARCH_X86_64 0
#endif

// =====================================================
// TIMER PORTÁVEL
// =====================================================
static inline uint64_t get_time_ns() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + t.tv_nsec;
}

// =====================================================
// FALLBACK (RODA EM QUALQUER CPU)
// =====================================================
uint32_t kernel_fallback(uint8_t *buf, size_t n) {
    uint32_t c = 0;
    for(size_t i=0;i<n;i++) {
        c ^= buf[i];
        for(int j=0;j<8;j++)
            c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
    }
    return c;
}

// =====================================================
// ARM64 + CRC + NEON
// =====================================================
#if ARCH_ARM64
uint32_t kernel_arm64(uint8_t *buf, size_t n) {
    uint32_t crc = 0;

    size_t i = 0;

    for (; i + 8 <= n; i += 8) {
        uint64_t v = *(uint64_t*)(buf + i);
        asm volatile("crc32x %w[c], %w[c], %x[v]"
            : [c] "+r"(crc)
            : [v] "r"(v));
    }

    for (; i < n; i++) {
        asm volatile("crc32b %w[c], %w[c], %w[v]"
            : [c] "+r"(crc)
            : [v] "r"(buf[i]));
    }

    return crc;
}
#endif

// =====================================================
// X86 (fallback otimizado simples)
// =====================================================
#if ARCH_X86_64
uint32_t kernel_x86(uint8_t *buf, size_t n) {
    uint32_t c = 0;
    for(size_t i=0;i<n;i++) {
        c = (c >> 1) ^ ((c ^ buf[i]) & 1 ? 0xEDB88320 : 0);
    }
    return c;
}
#endif

// =====================================================
// MAIN (ORQUESTRADOR)
// =====================================================
int main() {

    size_t n = 1 << 20; // 1MB
    static uint8_t buf[1<<20];

    // inicializar buffer determinístico
    for(size_t i=0;i<n;i++)
        buf[i] = (uint8_t)(i * 1315423911u);

    uint64_t t0 = get_time_ns();

    uint32_t crc = 0;

    // =========================
    // AUTO DISPATCH
    // =========================
#if ARCH_ARM64
    printf("[MODE] ARM64 CRC\n");
    crc = kernel_arm64(buf, n);

#elif ARCH_X86_64
    printf("[MODE] X86\n");
    crc = kernel_x86(buf, n);

#else
    printf("[MODE] FALLBACK\n");
    crc = kernel_fallback(buf, n);
#endif

    uint64_t t1 = get_time_ns();

    printf("CRC=%08x\n", crc);
    printf("TIME=%llu ns\n", (unsigned long long)(t1 - t0));
    printf("THROUGHPUT=%.2f MB/s\n",
        (double)n / (t1 - t0) * 1e3);

    return 0;
}
