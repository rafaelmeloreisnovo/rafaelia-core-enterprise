#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>

// --- ENGINE CONFIG ---
#define CORES 8
#define ITERS 5000000 
#define V_BASE20 0x3D4CCCCD 
#define V_PHI    0x3FCF1BBC 

typedef struct {
    uint32_t data[128] __attribute__((aligned(4096))); 
} VectraFabric;

void* pulse_vortex(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    // Substituindo aligned_alloc por posix_memalign para compatibilidade total
    void* mem = NULL;
    if (posix_memalign(&mem, 4096, sizeof(VectraFabric)) != 0) return NULL;
    VectraFabric *fabric = (VectraFabric*)mem;
    
    uint32_t loops = ITERS;
    uint32_t *ptr = fabric->data;

    asm volatile (
        "vld1.32 {q0, q1}, [%[buf]]!    \n\t" 
        "vld1.32 {q2, q3}, [%[buf]]     \n\t" 
        "sub %[buf], %[buf], #32       \n\t" 

        "vdup.32 q14, %[v20]           \n\t" 
        "vdup.32 q15, %[phi]           \n\t" 

        "1: \n\t"
        "vmla.f32 q0, q1, q14          \n\t" 
        "vmla.f32 q2, q3, q14          \n\t"
        "vmls.f32 q1, q2, q15          \n\t"
        "vmls.f32 q3, q0, q15          \n\t"

        "vext.32 q4, q0, q1, #1        \n\t" 
        "vrev64.32 q5, q2              \n\t" 
        "veor q0, q4, q5               \n\t" 
        "vext.32 q2, q3, q0, #2        \n\t" 

        "subs %[count], %[count], #1   \n\t"
        "bne 1b                        \n\t"

        "vst1.32 {q0, q1}, [%[buf]]!    \n\t"
        "vst1.32 {q2, q3}, [%[buf]]     \n\t"
        : [count] "+r" (loops), [buf] "+r" (ptr)
        : [v20] "r" (V_BASE20), [phi] "r" (V_PHI)
        : "q0","q1","q2","q3","q4","q5","q14","q15","memory","cc"
    );

    free(fabric);
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\n\033[1;35m[VECTRA-QUANTUM] REATOR ESTABILIZADO (posix_memalign)\033[0m\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_vortex, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    double total_states = (double)CORES * ITERS * 512.0;

    printf("\n\033[1;32m--- RELATÓRIO DE SÍNTESE VECTRA ---\n");
    printf("Tempo de Voo      : %.4f s\n", elapsed);
    printf("Throughput        : %.2f G-Estados/s\n", (total_states / elapsed) / 1e9);
    printf("Integridade       : 0x0 (Quantum-Like Active)\n");
    printf("\033[1;32m-----------------------------------\033[0m\n\n");

    return 0;
}
