#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ALPHA 0.25
#define N_HEADS 8
#define N_PATHS 64
#define N_ATTRACTORS 42
#define PI 3.14159265358979323846

typedef struct {
    double s[7];
    double C;
    double H;
    double phi;
    double state;
    uint64_t fnv1a64;
    uint32_t crc32;
    uint32_t entropy_milli;
    int attractor;
    int cycle42_ok;
} KernelState;

static uint32_t crc32_table[256];

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
}

static uint32_t crc32_run(const uint8_t *data, size_t len) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        c = crc32_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return ~c;
}

static uint64_t fnv1a64_run(const uint8_t *data, size_t len) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

static uint32_t entropy_milli_run(const uint8_t *data, size_t len) {
    if (len == 0) return 0;
    uint8_t seen[256] = {0};
    uint32_t unique = 0;
    uint32_t transitions = 0;

    seen[data[0]] = 1;
    unique = 1;
    for (size_t i = 1; i < len; ++i) {
        if (!seen[data[i]]) {
            seen[data[i]] = 1;
            unique++;
        }
        if (data[i] != data[i - 1]) transitions++;
    }

    uint32_t term_unique = (unique * 6000u) / 256u;
    uint32_t term_trans = (len > 1) ? (uint32_t)((transitions * 2000u) / (len - 1u)) : 0u;
    return term_unique + term_trans;
}

static double fract01(double x) {
    double y = x - floor(x);
    return (y < 0.0) ? (y + 1.0) : y;
}

static void toroidal_map(KernelState *ks, uint64_t h, uint32_t em, double c, double hh) {
    const double base[7] = {0.6180339887, 0.8660254038, 0.7071067812, 0.5773502691, 0.4142135623, 0.3010299957, 0.4342944819};
    double seed = (double)(h & 0xFFFFFFFFULL) / 4294967296.0 + (double)em / 10000.0 + c + hh;
    for (int i = 0; i < 7; ++i) {
        ks->s[i] = fract01(seed * base[i] + (double)i * 0.137 + (double)((h >> (i * 8)) & 0xFF) / 255.0);
    }
}

static double scheduler(double t) {
    return sin(2.0 * PI * 0.01 * t) + sin(2.0 * PI * 0.1 * t);
}

static double run_attention_paths(const KernelState *ks, double t) {
    double x = 0.0;
    for (int i = 0; i < 7; ++i) x += ks->s[i];
    x *= 0.142857142857;

    for (int h = 0; h < N_HEADS; ++h) {
        double op = x;
        switch (h % 4) {
            case 0: op = x + 0.05 * (h + 1); break;
            case 1: op = log(1.0 + fabs(x)) * (1.0 + 0.02 * h); break;
            case 2: op = sin(x * (h + 1)); break;
            default: op = sqrt(fabs(x) + 1e-9) * (1.0 + 0.01 * h); break;
        }
        x = 0.5 * x + 0.5 * op;
    }

    double p = scheduler(t);
    for (int path = 0; path < N_PATHS; ++path) {
        double gate = 1.0 + 0.1 * sin((double)(path + 1) * x + p);
        x *= gate;
        x = tanh(x);
    }
    return x;
}

static void step_kernel(KernelState *ks, double Cin, double Hin, double t) {
    ks->C = (1.0 - ALPHA) * ks->C + ALPHA * Cin;
    ks->H = (1.0 - ALPHA) * ks->H + ALPHA * Hin;

    if (ks->H < 0.0) ks->H = 0.0;
    if (ks->H > 1.0) ks->H = 1.0;

    ks->phi = (1.0 - ks->H) * ks->C;
    ks->state = run_attention_paths(ks, t) * ks->phi;

    uint32_t a = (uint32_t)(fabs(ks->state) * 1000000.0) % N_ATTRACTORS;
    ks->attractor = (int)a;
    ks->cycle42_ok = 1;
}

static int read_file(const char *path, uint8_t **out_buf, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }

    uint8_t *buf = NULL;
    size_t len = (size_t)sz;
    if (len > 0) {
        buf = (uint8_t *)malloc(len);
        if (!buf) { fclose(f); return -1; }
        if (fread(buf, 1, len, f) != len) { free(buf); fclose(f); return -1; }
    }
    fclose(f);

    *out_buf = buf;
    *out_len = len;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s <arquivo.bin> [steps]\n", argv[0]);
        return 1;
    }

    int steps = 42;
    if (argc >= 3) {
        steps = atoi(argv[2]);
        if (steps < 1) steps = 1;
    }

    uint8_t *buf = NULL;
    size_t len = 0;
    if (read_file(argv[1], &buf, &len) != 0) {
        fprintf(stderr, "erro: nao foi possivel ler '%s'\n", argv[1]);
        return 1;
    }

    crc32_init();

    KernelState ks;
    memset(&ks, 0, sizeof(ks));
    ks.C = 0.5;
    ks.H = 0.5;

    ks.fnv1a64 = fnv1a64_run(buf, len);
    ks.crc32 = crc32_run(buf, len);
    ks.entropy_milli = entropy_milli_run(buf, len);

    double Hin = fmin(1.0, (double)ks.entropy_milli / 10000.0);
    double Cin = 1.0 - Hin;
    toroidal_map(&ks, ks.fnv1a64, ks.entropy_milli, ks.C, ks.H);

    int attractor_start = -1;
    int attractor_after_42 = -2;

    for (int i = 0; i < steps; ++i) {
        step_kernel(&ks, Cin, Hin, (double)i);
        if (i == 0) attractor_start = ks.attractor;
        if (i == 41) attractor_after_42 = ks.attractor;
    }

    if (steps >= 42) ks.cycle42_ok = (attractor_start == attractor_after_42);

    printf("{\n");
    printf("  \"input\": {\n");
    printf("    \"file\": \"%s\",\n", argv[1]);
    printf("    \"bytes\": %zu\n", len);
    printf("  },\n");
    printf("  \"integrity\": {\n");
    printf("    \"fnv1a64\": \"0x%016llx\",\n", (unsigned long long)ks.fnv1a64);
    printf("    \"crc32\": \"0x%08x\"\n", ks.crc32);
    printf("  },\n");
    printf("  \"metrics\": {\n");
    printf("    \"entropy_milli\": %u,\n", ks.entropy_milli);
    printf("    \"C\": %.6f,\n", ks.C);
    printf("    \"H\": %.6f,\n", ks.H);
    printf("    \"phi\": %.6f\n", ks.phi);
    printf("  },\n");
    printf("  \"t7\": [%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f],\n",
           ks.s[0], ks.s[1], ks.s[2], ks.s[3], ks.s[4], ks.s[5], ks.s[6]);
    printf("  \"dynamics\": {\n");
    printf("    \"heads\": %d,\n", N_HEADS);
    printf("    \"paths\": %d,\n", N_PATHS);
    printf("    \"attractor\": %d,\n", ks.attractor);
    printf("    \"cycle42_ok\": %s\n", ks.cycle42_ok ? "true" : "false");
    printf("  }\n");
    printf("}\n");

    free(buf);
    return 0;
}
