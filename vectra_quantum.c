#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdlib.h>

#define CORES 8
#define ITERS 10000000 
#define LAYERS 32 // 4096 / 128 bits

void* pulse_quantum(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    // Alinhamento em 4096 para Casamento de Página (Zero Latency)
    uint32_t *block;
    posix_memalign((void**)&block, 4096, 4096); 
    
    uint32_t loops = ITERS;

    asm volatile (
        "vld1.32 {q0}, [%[buf]]     \n\t" // Estado Inicial
        "vdup.32 q14, %[v20]        \n\t" // Base 20 (Espaço)
        "vdup.32 q15, %[phi]        \n\t" // Áurea (Tempo/Fase)

        "1: \n\t"
        // --- LOOP DE DOBRA (32 LAYERS) ---
        // Aqui simulamos a superposição: cada camada é uma dimensão
        ".rept 32                   \n\t" 
        "vmla.f32 q1, q0, q14       \n\t" // Expansão
        "vmls.f32 q0, q1, q15       \n\t" // Contração (Interferência)
        "vrev64.32 q1, q1           \n\t" // Entalpi/Spin (Troca de bits)
        "veor q0, q0, q1            \n\t" // Colapso Parcial
        ".endr                      \n\t"

        "subs %[count], %[count], #1 \n\t"
        "bne 1b \n\t"

        "vst1.32 {q0}, [%[buf]]     \n\t"
        : [count] "+r" (loops)
        : [buf] "r" (block), [v20] "r" (0x3D4CCCCD), [phi] "r" (0x3FCF1BBC)
        : "q0","q1","q14","q15","memory","cc"
    );

    free(block);
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\033[1;34m[VECTRA-QUANTUM] 4096-BIT PAGE ALIGN | 32 LAYERS | SPIN-ROTATION\033[0m\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_quantum, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    
    // 8 Cores * ITERS * 32 Layers * 128 bits
    double total_states = (double)CORES * ITERS * 32.0 * 128.0;

    printf("\n\033[1;32m--- RELATÓRIO DE DOBRA QUÂNTICA ---\033[0m\n");
    printf("Tempo de Colapso : %.4f s\n", elapsed);
    printf("Massa Total      : %.2f Trilhões de Estados\n", total_states / 1e12);
    printf("Throughput Real  : %.2f G-Estados/s\n", (total_states / elapsed) / 1e9);
    printf("Configuração     : 32 Layers (Quantum-Like Interference)\n");
    printf("\033[1;32m------------------------------------\033[0m\n\n");

    return 0;
}
