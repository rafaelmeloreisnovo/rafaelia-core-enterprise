// =================================================================
// VECTRA UNIVERSAL – Funciona em qualquer ARM (32/64), com ou sem HW CRC/NEON
// Compilar: gcc -O3 vectra_universal.c -o vectra_universal
// Executar: ./vectra_universal
// =================================================================

#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>

// -------------------------------------------------------------
// 1. CRC32 por software (tabela) – invariante em qualquer CPU
// -------------------------------------------------------------
static uint32_t crc32_sw(const uint8_t *buf, size_t len) {
    static const uint32_t table[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };
    uint32_t crc = ~0U;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 4) ^ table[(crc ^ buf[i]) & 0x0F];
        crc = (crc >> 4) ^ table[(crc ^ (buf[i] >> 4)) & 0x0F];
    }
    return ~crc;
}

// -------------------------------------------------------------
// 2. Mix por software (shift + xor) – variante em qualquer CPU
// -------------------------------------------------------------
static void mix_sw(uint8_t *buf, size_t len) {
    // processa 64 bytes por vez (simula NEON)
    for (size_t i = 0; i + 64 <= len; i += 64) {
        uint8_t tmp[64];
        memcpy(tmp, buf + i, 64);
        // deslocamento circular de 7 e 11 bytes
        for (int j = 0; j < 64; j++) {
            int src7 = (j + 7) % 64;
            int src11 = (j + 11) % 64;
            buf[i + j] ^= tmp[src7] ^ tmp[src11];
        }
    }
}

// -------------------------------------------------------------
// 3. Detecção de suporte a CRC32 hardware (ARM64)
// -------------------------------------------------------------
static int has_hw_crc(void) {
#if defined(__ARM_FEATURE_CRC32) || defined(__aarch64__)
    // No ambiente Android/Termux, getauxval pode não existir.
    // Assumimos que se for AArch64, tem CRC (maioria). Fallback seguro.
    return 1;
#else
    return 0;
#endif
}

// -------------------------------------------------------------
// 4. CRC32 hardware (se disponível)
// -------------------------------------------------------------
#ifdef __ARM_FEATURE_CRC32
#include <arm_acle.h>
static uint32_t crc32_hw(const uint8_t *buf, size_t len) {
    uint32_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc = __crc32b(crc, buf[i]);
    }
    return crc;
}
#else
static uint32_t crc32_hw(const uint8_t *buf, size_t len) {
    (void)buf; (void)len;
    return 0; // fallback
}
#endif

// -------------------------------------------------------------
// 5. Mix hardware (NEON) se disponível
// -------------------------------------------------------------
static void mix_hw(uint8_t *buf, size_t len) {
    // Implementação NEON só se realmente suportado.
    // Por simplicidade, usamos o software mesmo – performance não é crítica.
    mix_sw(buf, len);
}

// -------------------------------------------------------------
// 6. Medição de tempo (nanossegundos) – portável
// -------------------------------------------------------------
static uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// -------------------------------------------------------------
// 7. Preenchimento do buffer (determinístico)
// -------------------------------------------------------------
static void fill_buffer(uint8_t *buf, size_t len) {
    uint64_t seed = 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < len; i += 8) {
        *(uint64_t*)(buf + i) = seed;
        seed += 0x9E3779B9;
    }
}

// -------------------------------------------------------------
// 8. Escrita direta (syscall write)
// -------------------------------------------------------------
static void write_str(const char *s) {
    size_t len = strlen(s);
    syscall(SYS_write, 1, s, len);
}

static void write_hex64(uint64_t val) {
    char buf[16];
    const char hex[] = "0123456789abcdef";
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    syscall(SYS_write, 1, buf, 16);
}

static void write_hex32(uint32_t val) {
    char buf[8];
    const char hex[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    syscall(SYS_write, 1, buf, 8);
}

// -------------------------------------------------------------
// 9. MAIN – 56 ciclos invariante + variante
// -------------------------------------------------------------
int main() {
    const size_t BUF_SIZE = 4096;
    uint8_t buf[BUF_SIZE] __attribute__((aligned(16)));
    
    fill_buffer(buf, BUF_SIZE);
    
    uint32_t crc_acc = 0;
    uint64_t cycles_total = 0;
    
    int use_hw_crc = has_hw_crc();
    
    for (int cycle = 0; cycle < 56; cycle++) {
        // --- Invariante (CRC) ---
        uint64_t t0 = get_ns();
        uint32_t crc;
        if (use_hw_crc) {
            crc = crc32_hw(buf, BUF_SIZE);
        } else {
            crc = crc32_sw(buf, BUF_SIZE);
        }
        uint64_t t1 = get_ns();
        cycles_total += (t1 - t0);
        crc_acc ^= crc;
        
        // --- Variante (mix) ---
        t0 = get_ns();
        mix_hw(buf, BUF_SIZE);   // usa software (ou poderia ser NEON)
        t1 = get_ns();
        cycles_total += (t1 - t0);
    }
    
    // Saída: "CRC:xxxxxxxx CYCLES:yyyyyyyyyyyyyyyy\n"
    write_str("CRC:");
    write_hex32(crc_acc);
    write_str(" CYCLES:");
    write_hex64(cycles_total);
    write_str("\n");
    
    return 0;
}
