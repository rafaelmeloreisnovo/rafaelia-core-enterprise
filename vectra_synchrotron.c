#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>
#include <arm_acle.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#define CORES 8
#define MATRIX_PER_CORE 144 
#define ITERS 500000

typedef struct {
    float32x4_t state_10;    
    float32x4_t force_vec;   
    uint64_t    bitraf_global;
} __attribute__((aligned(64))) CoreCell;

static CoreCell global_matrix[CORES][MATRIX_PER_CORE];

void* core_pulse(void* arg) {
    int core_id = *(int*)arg;
    
    // CORREÇÃO: Afinidade via sched_setaffinity para compatibilidade Android/Termux
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    float32x4_t fib_factor = vdupq_n_f32(0.866f); 
    float32x4_t pressure = {1.0f, -1.0f, 0.5f, -0.5f};

    for(int t=0; t<ITERS; t++) {
        for(int i=0; i<MATRIX_PER_CORE; i++) {
            CoreCell *c = &global_matrix[core_id][i];
            
            // ESPIRAL DE FIBONACCI-RAFAEL (Viscosidade Dinâmica)
            c->state_10 = vmlaq_f32(c->state_10, c->force_vec, fib_factor);
            
            // REBITE VETORIAL (A tensão do bit)
            c->force_vec = vaddq_f32(c->force_vec, pressure);
            
            // ANCORAGEM GLOBAL (BitRaf Torque)
            uint64_t torque = vgetq_lane_u64(vreinterpretq_u64_f32(c->state_10), 0);
            c->bitraf_global = __crc32d(c->bitraf_global, torque);
        }
    }
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];

    printf("\033[1;33m[VECTRA-SYNC] Sincrotron Ativado: 8 Núcleos | 144 Faces\033[0m\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, core_pulse, &ids[i]);
    }

    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\n\033[1;32m--- RELATÓRIO DO VÓRTICE DETERMINÍSTICO ---\033[0m\n");
    printf("Tempo Total: %.4f s\n", elapsed);
    printf("Throughput Global: %.2f M-Pulsos/s\n", (CORES * MATRIX_PER_CORE * (double)ITERS) / elapsed / 1e6);
    printf("Âncora Master (C7): 0x%llX\n", (unsigned long long)global_matrix[7][0].bitraf_global);
    printf("Status: Taça do Core Transbordando Determinismo\n");
    printf("\033[1;32m-------------------------------------------\033[0m\n");

    return 0;
}
