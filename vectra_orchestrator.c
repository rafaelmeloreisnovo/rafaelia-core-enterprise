#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>

// ============================
// TIMER (ARM64)
// ============================
static inline uint64_t rdtsc() {
    uint64_t v;
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

int main() {
    int fd = open("input.bin", O_RDONLY);
    if (fd < 0) return 1;

    struct stat st;
    if (fstat(fd, &st) < 0) return 1;

    size_t size = st.st_size;

    uint8_t *buf = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == (void*)-1) return 1;

    uint64_t out_crc = 0;

    uint64_t t0 = rdtsc();

    // ============================
    // ORQUESTRADOR (3 CAMINHOS)
    // ============================
    asm volatile(

        // x0 = buffer
        // x1 = size
        // x2 = crc
        // x3 = delta

        "mov x0, %[buf]\n"
        "mov x1, %[size]\n"
        "mov x2, #0\n"
        "mov x3, #0\n"

        "cmp x1, #64\n"
        "blt END\n"

        "LOOP:\n"

        // carregar 64 bytes
        "ld1 {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], #64\n"

        // ============================
        // calcular delta simples
        // ============================
        "umov x4, v0.d[0]\n"
        "eor x3, x3, x4\n"

        // ============================
        // VÁLVULA (decisão dinâmica)
        // ============================

        "cmp x3, #0x1000\n"
        "blt PATH1\n"

        "cmp x3, #0x100000\n"
        "blt PATH2\n"

        "b PATH3\n"

        // ============================
        // PATH 1: CRC puro
        // ============================
        "PATH1:\n"

        "umov x5, v0.d[0]\n"
        "crc32x w2, w2, x5\n"

        "umov x5, v1.d[0]\n"
        "crc32x w2, w2, x5\n"

        "b NEXT\n"

        // ============================
        // PATH 2: NEON MIX + CRC
        // ============================
        "PATH2:\n"

        "eor v0.16b, v0.16b, v1.16b\n"
        "eor v2.16b, v2.16b, v3.16b\n"

        "umov x5, v0.d[0]\n"
        "crc32x w2, w2, x5\n"

        "umov x5, v2.d[0]\n"
        "crc32x w2, w2, x5\n"

        "b NEXT\n"

        // ============================
        // PATH 3: TURBULÊNCIA
        // ============================
        "PATH3:\n"

        "ext v0.16b, v0.16b, v1.16b, #7\n"
        "ext v2.16b, v2.16b, v3.16b, #11\n"

        "eor v0.16b, v0.16b, v2.16b\n"

        "umov x5, v0.d[1]\n"
        "crc32x w2, w2, x5\n"

        "NEXT:\n"

        "subs x1, x1, #64\n"
        "bge LOOP\n"

        "END:\n"
        "mov %[out], x2\n"

        : [out] "=r"(out_crc)
        : [buf] "r"(buf), [size] "r"(size)
        : "x0","x1","x2","x3","x4","x5",
          "v0","v1","v2","v3","memory"
    );

    uint64_t t1 = rdtsc();

    char out[128];
    int len = 0;

    len += sprintf(out+len, "CRC=%llx\n", (unsigned long long)out_crc);
    len += sprintf(out+len, "CYCLES=%llu\n", (unsigned long long)(t1 - t0));

    write(1, out, len);

    return 0;
}
