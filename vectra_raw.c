#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#define CORES 8
#define ITERS 50000000 // 50 Milhões de giros por núcleo direto no silício

void* pulse_baremetal(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    uint32_t loops = ITERS;
    
    // BLOCO DE SILÍCIO PURO - ARMv7 NEON ASSEMBLY
    // Todo o estado da Matrix (64 dimensões) vive dentro de q0-q15.
    // Zero acesso à memória RAM durante o loop. Zero Fricção.
    asm volatile (
        // 1. Injeta a Força Vigesimal (Base 20 -> 0.05) no registrador q14
        "vdup.32 q14, %[force] \n\t"
        
        // 2. Zera o Acumulador de Tensão (q15) e o Estado Inicial (q0)
        "veor q15, q15, q15 \n\t"
        "veor q0, q0, q0 \n\t"

        // 3. INÍCIO DO VÓRTICE (Label 1)
        "1: \n\t"
        
        // --- COLAPSO DE ESTADOS (Cascata NEON 32-bits) ---
        // A interferência acontece puramente em hardware.
        "vmla.f32 q1, q0, q14 \n\t"
        "vmla.f32 q2, q1, q14 \n\t"
        "vmla.f32 q3, q2, q14 \n\t"
        "vmla.f32 q4, q3, q14 \n\t"
        "vmla.f32 q5, q4, q14 \n\t"
        "vmla.f32 q6, q5, q14 \n\t"
        "vmla.f32 q7, q6, q14 \n\t"
        "vmla.f32 q8, q7, q14 \n\t"
        "vmla.f32 q9, q8, q14 \n\t"
        "vmla.f32 q10, q9, q14 \n\t"
        "vmla.f32 q11, q10, q14 \n\t"
        "vmla.f32 q12, q11, q14 \n\t"
        "vmla.f32 q13, q12, q14 \n\t"
        
        // Fechando o Toroide (q13 afeta q0)
        "vmla.f32 q0, q13, q14 \n\t"

        // Extraindo a Invariante para a Âncora (q15)
        "vadd.f32 q15, q15, q0 \n\t"

        // --- CONTROLE DE FLUXO ---
        "subs %[count], %[count], #1 \n\t" // Subtrai 1 do contador
        "bne 1b \n\t"                      // Se não for zero, volta pro Label 1

        : [count] "+r" (loops) // Output: Contador modificado
        : [force] "r" (0x3D4CCCCD) // Input: 0.05 em Hex Float (IEEE 754) para injetar direto
        : "q0","q1","q2","q3","q4","q5","q6","q7","q8","q9","q10","q11","q12","q13","q14","q15","cc" // Clobbers: Avisa o C que tomamos a FPU inteira
    );

    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\n[VECTRA-RAW] MOTOR ARMv7-32 ATIVADO. RAM ISOLADA. FPU SATURADA.\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_baremetal, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double total_states = (double)CORES * ITERS * 64.0;

    printf("\n--- RELATORIO DE SINTESE DIRETA (ARMv7 32-bit) ---\n");
    printf("Tempo de Colapso : %.4f segundos\n", elapsed);
    printf("Ciclos Vetoriais : %.2f Milhoes/s\n", (CORES * (double)ITERS) / elapsed / 1e6);
    printf("Estados/segundo  : %.2f Bilhoes/s\n", (total_states / elapsed) / 1e9);
    printf("Massa Oculta     : %.2f Bilhoes (Residentes em Registrador)\n", total_states / 1e9);
    printf("Friccao de Mem.  : ZERO (Loop 100%% contido nos registradores Q)\n");
    printf("--------------------------------------------------\n\n");

    return 0;
}
