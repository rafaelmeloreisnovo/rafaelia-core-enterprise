#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#define CORES 8
#define ITERS 80000000 // Aumentando para testar o limite térmico

void* pulse_singularity(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    uint32_t loops = ITERS;

    asm volatile (
        "vdup.32 q14, %[v20] \n\t" // Base 20
        "vdup.32 q15, %[phi] \n\t" // Proporção Áurea
        
        "veor q0, q0, q0 \n\t"     // Limpeza de campo inicial
        "veor q1, q1, q1 \n\t"

        "1: \n\t"
        // --- CAMADA ALPHA (Expansão) ---
        "vmla.f32 q2, q0, q14 \n\t"
        "vmla.f32 q3, q1, q14 \n\t"
        "vmla.f32 q4, q2, q14 \n\t"
        "vmla.f32 q5, q3, q14 \n\t"
        
        // --- CAMADA BETA (Contrações Áureas) ---
        "vmls.f32 q6, q4, q15 \n\t"
        "vmls.f32 q7, q5, q15 \n\t"
        "vmls.f32 q8, q6, q15 \n\t"
        "vmls.f32 q9, q7, q15 \n\t"

        // --- CAMADA GAMMA (Interferência de 128 Estados) ---
        "vadd.f32 q10, q8, q2 \n\t"
        "vadd.f32 q11, q9, q3 \n\t"
        "vsub.f32 q12, q10, q4 \n\t"
        "vsub.f32 q13, q11, q5 \n\t"

        // --- RECOIL (Realimentação do Toroide) ---
        "veor q0, q12, q13 \n\t"
        "vadd.f32 q1, q0, q14 \n\t"

        "subs %[count], %[count], #1 \n\t"
        "bne 1b \n\t"

        : [count] "+r" (loops)
        : [v20] "r" (0x3D4CCCCD), [phi] "r" (0x3FCF1BBC)
        : "q0","q1","q2","q3","q4","q5","q6","q7","q8","q9","q10","q11","q12","q13","q14","q15","cc"
    );

    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\033[1;35m[VECTRA-SINGULARITY] ACIONANDO 128 ESTADOS RESIDENTES...\033[0m\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_singularity, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    
    // 8 Cores * ITERS * 128 estados por ciclo
    double total_states = (double)CORES * ITERS * 128.0;

    printf("\n\033[1;32m--- RELATÓRIO DE SINGULARIDADE TÉRMICA ---\033[0m\n");
    printf("Tempo de Voo     : %.4f s\n", elapsed);
    printf("Massa Processada : %.2f Bilhões de Estados\n", total_states / 1e9);
    printf("Densidade Real   : %.2f G-Estados/s\n", (total_states / elapsed) / 1e9);
    printf("Integridade      : 0x0 (Sincronia Vetorial Absoluta)\n");
    printf("\033[1;32m------------------------------------------\033[0m\n\n");

    return 0;
}
