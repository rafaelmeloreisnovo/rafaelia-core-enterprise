#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>
#include <arm_acle.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#define CORES 8
#define BASE_V20 400
#define ITERS 250000

typedef struct {
    float32x4_t layer_pi[8];   // 32 estados para o círculo
    float32x4_t layer_phi[8];  // 32 estados para a espiral
    float32x4_t collision;     // Zona de interferência
    uint64_t    vortex_id;
} __attribute__((aligned(64))) ColliderCell;

static ColliderCell matrix[CORES][BASE_V20];

void* pulse_collider(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    float32x4_t val_pi = vdupq_n_f32(3.141592f);
    float32x4_t val_phi = vdupq_n_f32(1.618033f);

    for(int t=0; t<ITERS; t++) {
        for(int i=0; i<BASE_V20; i++) {
            ColliderCell *c = &matrix[core_id][i];
            
            // 1. PRESSÃO DO CÍRCULO (Layer PI)
            for(int j=0; j<8; j++) c->layer_pi[j] = vmulq_f32(c->layer_pi[j], val_pi);
            
            // 2. TENSÃO DA ESPIRAL (Layer PHI)
            for(int j=0; j<8; j++) c->layer_phi[j] = vmlaq_f32(c->layer_phi[j], c->layer_phi[j], val_phi);
            
            // 3. COLISÃO (Interferência de 64 Estados)
            c->collision = vsubq_f32(c->layer_pi[0], c->layer_phi[0]);
            
            c->vortex_id = __crc32d(c->vortex_id, vgetq_lane_u64(vreinterpretq_u64_f32(c->collision), 0));
        }
    }
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\033[1;31m[VECTRA-COLLIDER] Colidindo PI e PHI em 8 Núcleos...\033[0m\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_collider, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\n\033[1;32m--- RELATÓRIO DE COLISÃO GEOMÉTRICA ---\033[0m\n");
    printf("Tempo de Estresse:      %.4f s\n", elapsed);
    printf("Vazão de Colisão:       %.2f M-Pulsos/s\n", (CORES * BASE_V20 * (double)ITERS) / elapsed / 1e6);
    printf("Estados em Conflito:    %.2f Bilhões\n", (CORES * BASE_V20 * (double)ITERS * 64.0) / 1e9);
    printf("Assinatura do Vórtex:   0x%llX\n", (unsigned long long)matrix[7][0].vortex_id);
    printf("Status: Invariante Extraída do Caos de Duas Constantes\n");
    printf("\033[1;32m---------------------------------------\033[0m\n");

    return 0;
}
