#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define CORES 8
#define ITERS 20000000 // 20 Milhões por vetor por núcleo

void* pulse_extreme(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    uint32_t loops = ITERS;
    uint32_t chaos[16]; // 16*32 bits = 512 bits de caos inicial

    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, chaos, 64);
    close(fd);

    asm volatile (
        "vld1.32 {q0-q3}, [%[chaos]] \n\t"  // Carrega caos inicial
        "veor q15, q15, q15 \n\t"           // Zera acumulador final
        "vdup.32 q14, %[force] \n\t"        // Força base 20

        "1: \n\t"
        // BLOCO DE 4 MATRIZES RESIDENTES (q0-q3, q4-q7, q8-q11, q12-q15)
        "vmla.f32 q1, q0, q14 \n\t"
        "vmla.f32 q2, q1, q14 \n\t"
        "vmla.f32 q3, q2, q14 \n\t"
        "vmla.f32 q0, q3, q14 \n\t"

        "vmla.f32 q5, q4, q14 \n\t"
        "vmla.f32 q6, q5, q14 \n\t"
        "vmla.f32 q7, q6, q14 \n\t"
        "vmla.f32 q4, q7, q14 \n\t"

        "vmla.f32 q9, q8, q14 \n\t"
        "vmla.f32 q10, q9, q14 \n\t"
        "vmla.f32 q11, q10, q14 \n\t"
        "vmla.f32 q8, q11, q14 \n\t"

        "vmla.f32 q13, q12, q14 \n\t"
        "vmla.f32 q14, q13, q14 \n\t"
        "vmla.f32 q15, q14, q14 \n\t"
        "vmla.f32 q12, q15, q14 \n\t"

        "subs %[count], %[count], #1 \n\t"
        "bne 1b \n\t"

        : [count] "+r" (loops)
        : [force] "r" (0x3D4CCCCD), [chaos] "r" (chaos)
        : "q0","q1","q2","q3","q4","q5","q6","q7",
          "q8","q9","q10","q11","q12","q13","q14","q15","cc","memory"
    );

    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("[VECTRA-EXTREME] MULTI-MATRIZ ARMv7-32 RESIDENTE INICIADO\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_extreme, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    double total_states = (double)CORES * ITERS * 64.0 * 4.0; // 4 matrizes por thread

    printf("\n--- RELATORIO EXTREMO ---\n");
    printf("Tempo de Colapso : %.4f s\n", elapsed);
    printf("Ciclos Vetoriais : %.2f Milhoes/s\n", (CORES*(double)ITERS*4)/elapsed/1e6);
    printf("Estados/segundo  : %.2f Bilhoes/s\n", (total_states/elapsed)/1e9);
    printf("Massa Oculta     : %.2f Bilhoes (Residentes em Registrador)\n", total_states/1e9);
    printf("Friccao de Mem.  : ZERO ✅ Loop 100%% contido nos registradores Q\n");
    printf("-----------------------------------------\n\n");

    return 0;
}
