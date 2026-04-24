#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>
#include <arm_acle.h>
#include <sched.h>
#include <time.h>

#define MATRIX_SIZE 512
#define ITERS 2000000 // Dobramos o estresse para 2M de ciclos

typedef struct {
    float32x4_t geometry;
    uint64_t    bitraf_id;
    uint8_t     pad[40];
} __attribute__((aligned(64))) HyperCell;

static HyperCell matrix[MATRIX_SIZE];

// Inicialização com ruído para evitar Checksum 0
void seed_matrix() {
    for(int i=0; i<MATRIX_SIZE; i++) {
        matrix[i].geometry = vdupq_n_f32((float)i * 0.001f);
        matrix[i].bitraf_id = 0xABCDEF;
    }
}

static inline void pulse_interaction(int i, float32x4_t w) {
    HyperCell *c = &matrix[i];
    // Pega o vizinho (com wrap-around para manter o Toro)
    HyperCell *prev = &matrix[(i - 1 + MATRIX_SIZE) % MATRIX_SIZE];

    __builtin_prefetch(c + 2, 1, 3);

    // [INTERAÇÃO FRACTAL]
    // O novo estado é: (Estado Atual + Estado do Vizinho) * Pesos
    float32x4_t interaction = vaddq_f32(c->geometry, prev->geometry);
    c->geometry = vmlaq_f32(c->geometry, interaction, w);

    // Integridade BitRaf
    uint64_t data = vgetq_lane_u64(vreinterpretq_u64_f32(c->geometry), 0);
    c->bitraf_id = __crc32d(c->bitraf_id, data);
}

int main() {
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(7, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    seed_matrix();
    float32x4_t weights = vdupq_n_f32(0.00001f);
    printf("\033[1;33m[VECTRA v8] Ativando Interação de Vizinhos (Toro 7D)...\033[0m\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int t=0; t<ITERS; t++) {
        for(int i=0; i<MATRIX_SIZE; i++) {
            pulse_interaction(i, weights);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\n\033[1;32m--- BENCHMARK DE IMPULSOS VECTRA ---\033[0m\n");
    printf("Tempo de Execução: %.4f s\n", total_time);
    printf("Impulsos Totais: %.0f\n", (double)MATRIX_SIZE * ITERS);
    printf("Throughput Final: %.2f M-Pulsos/s\n", (MATRIX_SIZE * (double)ITERS) / total_time / 1e6);
    printf("BitRaf Master Sign: 0x%llX\n", (unsigned long long)matrix[0].bitraf_id);
    printf("\033[1;32m------------------------------------\033[0m\n");

    return 0;
}
