#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>
#include <arm_acle.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <math.h>

#define CORES 8
#define BASE_V20 400
#define LAYERS 4
#define ITERS 200000

typedef struct {
    float32x4_t states[16];
    float32x4_t elasticity;
    uint64_t    anchor;
} __attribute__((aligned(64))) ObsCell;

static ObsCell global_matrix[CORES][BASE_V20];

void* pulse_observer(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    float32x4_t shadow_fact = vdupq_n_f32(1.618f); // Proporção Áurea como Fato

    for(int t=0; t<ITERS; t++) {
        for(int i=0; i<BASE_V20; i++) {
            ObsCell *c = &global_matrix[core_id][i];
            
            // Ciclo Adaptativo de 64 Estados
            float32x4_t diff = vsubq_f32(c->states[0], shadow_fact);
            c->elasticity = vmulq_f32(diff, vdupq_n_f32(0.01f));

            for(int s=0; s<16; s++) {
                // Evolução por Interferência
                c->states[s] = vmlaq_f32(c->states[s], c->elasticity, shadow_fact);
            }

            c->anchor = __crc32d(c->anchor, vgetq_lane_u64(vreinterpretq_u64_f32(c->states[0]), 0));
        }
    }
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\033[1;35m[VECTRA-OBSERVER] Iniciando Varredura Multidimensional...\033[0m\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_observer, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // Métricas Calculadas
    double total_pulsos = (double)CORES * BASE_V20 * ITERS;
    double total_estados = total_pulsos * 64.0;
    double throughput = (total_pulsos / elapsed) / 1e6;
    double info_density = (total_estados / elapsed) / 1e9;

    printf("\n\033[1;32m--- PAINEL DE MÉTRICAS UNIFICADAS ---\033[0m\n");
    printf("1. TEMPO DE VÔO:         %.4f s\n", elapsed);
    printf("2. VAZÃO VETORIAL:       %.2f M-Pulsos/s\n", throughput);
    printf("3. DENSIDADE CRISTALINA: %.2f G-Estados/s\n", info_density);
    printf("4. MASSA TOTAL:          %.2f Bilhões de Estados\n", total_estados / 1e9);
    printf("5. HARMONIA (C7):        0x%llX\n", (unsigned long long)global_matrix[7][0].anchor);
    
    if(global_matrix[7][0].anchor == 0) 
        printf("6. STATUS:               SINGULARIDADE ALCANÇADA (0x0)\n");
    else 
        printf("6. STATUS:               FLUXO EM TENSÃO ADAPTATIVA\n");
    
    printf("\033[1;32m-------------------------------------\033[0m\n");

    return 0;
}
