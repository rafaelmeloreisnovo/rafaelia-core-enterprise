#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>

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

    asm volatile(

        "mov x9, %[buf]\n"
        "mov x10, %[size]\n"
        "mov w11, wzr\n"     // crc
        "mov x12, xzr\n"     // delta

        "cmp x10, #64\n"
        "blt END\n"

        "LOOP:\n"

        "ld1 {v0.16b, v1.16b, v2.16b, v3.16b}, [x9], #64\n"

        // delta
        "umov x13, v0.d[0]\n"
        "eor x12, x12, x13\n"

        // valve
        "cmp x12, #0x1000\n"
        "blt P1\n"

        "cmp x12, #0x100000\n"
        "blt P2\n"

        "b P3\n"

        // PATH1
        "P1:\n"
        "umov x14, v0.d[0]\n"
        "crc32x w11, w11, x14\n"
        "b NEXT\n"

        // PATH2
        "P2:\n"
        "eor v0.16b, v0.16b, v1.16b\n"
        "umov x14, v0.d[0]\n"
        "crc32x w11, w11, x14\n"
        "b NEXT\n"

        // PATH3
        "P3:\n"
        "ext v0.16b, v0.16b, v1.16b, #7\n"
        "ext v2.16b, v2.16b, v3.16b, #11\n"
        "eor v0.16b, v0.16b, v2.16b\n"
        "umov x14, v0.d[1]\n"
        "crc32x w11, w11, x14\n"

        "NEXT:\n"
        "subs x10, x10, #64\n"
        "bge LOOP\n"

        "END:\n"
        "mov %[out], x11\n"

        : [out] "=r"(out_crc)
        : [buf] "r"(buf), [size] "r"(size)
        : "memory"
    );

    uint64_t t1 = rdtsc();

    char out[128];
    int len = 0;

    len += sprintf(out+len, "CRC=%llx\n", (unsigned long long)out_crc);
    len += sprintf(out+len, "CYCLES=%llu\n", (unsigned long long)(t1 - t0));

    write(1, out, len);

    return 0;
}
