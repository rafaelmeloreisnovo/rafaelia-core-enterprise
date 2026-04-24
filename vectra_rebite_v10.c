#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>
#include <arm_acle.h>
#include <sched.h>
#include <time.h>
#include <math.h>

#define MATRIX_SIZE 512
#define ITERS 2000000

typedef struct {
    float32x4_t tensor_force; // Força e Direção (X, Y, Z, Magnitude)
    float32x4_t phase_states; // Os 10 estados de rebite (distribuídos)
    uint64_t    bitraf_anchor; // O prego (rebite) de integridade
} __attribute__((aligned(64))) RebiteCell;

static RebiteCell matrix[MATRIX_SIZE];

static inline void apply_rebite(int i, float32x4_t external_pressure) {
    RebiteCell *c = &matrix[i];
    
    // 1. TENSÃO DO VETOR (Magnitude da Força)
    // Calculamos a direção para onde o bit "quer" colapsar
    c->tensor_force = vmlaq_f32(c->tensor_force, external_pressure, vdupq_n_f32(0.618f));
    
    // 2. REBITE DE 10 ESTADOS (Empilhamento de Fase)
    // Misturamos a força com a fase para ver se o bit "resiste"
    c->phase_states = vaddq_f32(c->phase_states, vrev64q_f32(c->tensor_force));
    
    // 3. DIREÇÃO DA MATRIZ (Ancoragem)
    // O BitRaf aqui não é um CRC comum, é o sensor de torque do vetor
    uint64_t torque = vgetq_lane_u64(vreinterpretq_u64_f32(c->phase_states), 0);
    c->bitraf_anchor = __crc32d(c->bitraf_anchor, torque);
}

int main() {
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(7, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    // Pressão externa: simulando um bit vindo de um processo "nada a ver"
    float32x4_t pressure = {1.0f, 0.0f, -1.0f, 0.5f}; 

    printf("\033[1;31m[VECTRA-REBITE] Forçando 10 Estados e Tensão Vetorial...\033[0m\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int t=0; t<ITERS; t++) {
        for(int i=0; i<MATRIX_SIZE; i++) {
            apply_rebite(i, pressure);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\n\033[1;32m--- BENCHMARK DE FORÇA VETORIAL (TOTAL) ---\033[0m\n");
    printf("Tempo Total: %.4f s\n", elapsed);
    printf("Tamanho da Força: %.2f G-Impulsos\n", (MATRIX_SIZE * (double)ITERS) / 1e9);
    printf("Throughput de Rebite: %.2f M-Pulsos/s\n", (MATRIX_SIZE * (double)ITERS) / elapsed / 1e6);
    printf("Âncora BitRaf (Direção): 0x%llX\n", (unsigned long long)matrix[0].bitraf_anchor);
    printf("Status: Invariante Geométrica Presa no Silício\n");
    printf("\033[1;32m-------------------------------------------\033[0m\n");

    return 0;
}
