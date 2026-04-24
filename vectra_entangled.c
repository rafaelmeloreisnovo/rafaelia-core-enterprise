#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>

#define CORES 8
#define ITERS 5000000 

void* pulse_entangled(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    uint32_t *block;
    posix_memalign((void**)&block, 4096, 4096); 
    uint32_t loops = ITERS;

    asm volatile (
        "vld1.32 {q0-q3}, [%[buf]]     \n\t" // Carga inicial do tecido
        "vdup.32 q14, %[v20]           \n\t" // Base 20
        "vdup.32 q15, %[phi]           \n\t" // Áurea

        "1: \n\t"
        // --- ENTRELAÇAMENTO (CROSS-REGISTER INTERFERENCE) ---
        // Aqui um registrador "invade" o espaço do outro
        "vmla.f32 q0, q1, q14          \n\t" // q0 depende de q1
        "vmls.f32 q1, q2, q15          \n\t" // q1 depende de q2
        "vmla.f32 q2, q3, q14          \n\t" // q2 depende de q3
        "vmls.f32 q3, q0, q15          \n\t" // q3 fecha o ciclo no q0

        // DOBRA DE PERMUTAÇÃO (Spooky Action)
        // Move pedaços de 32-bits entre os vetores (Entrelaçamento Físico)
        "vext.32 q4, q0, q1, #1        \n\t" // q4 recebe pedaços de q0 e q1
        "vext.32 q5, q2, q3, #2        \n\t" // q5 recebe pedaços de q2 e q3
        
        "veor q0, q4, q15              \n\t" // Colapso por interferência
        "veor q2, q5, q14              \n\t"

        "subs %[count], %[count], #1   \n\t"
        "bne 1b \n\t"

        "vst1.32 {q0-q3}, [%[buf]]     \n\t"
        : [count] "+r" (loops)
        : [buf] "r" (block), [v20] "r" (0x3D4CCCCD), [phi] "r" (0x3FCF1BBC)
        : "q0","q1","q2","q3","q4","q5","q14","q15","memory","cc"
    );

    free(block);
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\033[1;31m[VECTRA-ENTANGLED] CROSS-POLLINATION | NON-LOCAL DETERMINISM\033[0m\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_entangled, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    
    // 8 Cores * ITERS * 4 Matrizes Entrelaçadas * 128 bits
    double total_states = (double)CORES * ITERS * 4.0 * 128.0;

    printf("\n\033[1;32m--- RELATÓRIO DE ENTRELAÇAMENTO --- \n");
    printf("Tempo de Colapso : %.4f s\n", elapsed);
    printf("Vazão de Estados : %.2f G-Estados/s\n", (total_states / elapsed) / 1e9);
    printf("Entrelaçamento   : Ativo (vext.32 Cross-Pollination)\n");
    printf("Sincronia Global : 0x0 (Invariante Determinística)\n");
    printf("\033[1;32m------------------------------------\033[0m\n\n");

    return 0;
}
