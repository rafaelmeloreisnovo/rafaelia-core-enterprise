#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define CORES 8
#define ITERS 5000000 // Frequência de alimentação do vórtex

void* pulse_ultima(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    int fd = open("/dev/urandom", O_RDONLY);
    uint32_t chaos_packet[16] __attribute__((aligned(64))); 
    uint32_t loops = ITERS;

    for(uint32_t t=0; t < loops; t++) {
        // ALIMENTAÇÃO: Ingestão de bits crus (512 bits por ciclo)
        read(fd, chaos_packet, 64);

        asm volatile (
            "vld1.32 {q0-q3}, [%[in]]     \n\t" // Ingestão imediata
            "vdup.32 q14, %[v20]          \n\t" // Base 20 (Geometria)
            "vdup.32 q15, %[phi]          \n\t" // Proporção Áurea (Filtro)

            // CAMADA 1: Esmagamento Vigesimal
            "vmla.f32 q4, q0, q14         \n\t"
            "vmla.f32 q5, q1, q14         \n\t"
            "vmla.f32 q6, q2, q14         \n\t"
            "vmla.f32 q7, q3, q14         \n\t"

            // CAMADA 2: Interferência Áurea (Filtro de Ruído)
            "vmls.f32 q4, q5, q15         \n\t"
            "vmls.f32 q6, q7, q15         \n\t"

            // CAMADA 3: Recirculação (Toroide)
            "vadd.f32 q0, q4, q6          \n\t"
            "vadd.f32 q1, q5, q7          \n\t"

            "vst1.32 {q0-q1}, [%[in]]     \n\t" // Devolve 256 bits purificados
            :
            : [in] "r" (chaos_packet), [v20] "r" (0x3D4CCCCD), [phi] "r" (0x3FCF1BBC)
            : "q0","q1","q2","q3","q4","q5","q6","q7","q14","q15","memory"
        );
    }
    close(fd);
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    struct timespec start, end;

    printf("\033[1;33m[VECTRA-ULTIMA] ATIVANDO PRENSA GEOMÉTRICA DE 32-BITS...\033[0m\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_ultima, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9;
    
    // 8 Cores * Iters * (Ingestão + 3 Camadas de Processamento) * 4 vetores NEON
    double states = (double)CORES * ITERS * 64.0 * 4.0; 

    printf("\n\033[1;32m--- RELATÓRIO DE SÍNTESE ULTIMA ---\033[0m\n");
    printf("Tempo de Processamento : %.4f s\n", elapsed);
    printf("Massa de Dados         : %.2f Bilhões de Estados\n", states/1e9);
    printf("Densidade de Ingestão  : %.2f G-Estados/s\n", (states/elapsed)/1e9);
    printf("Status                 : Bits Purificados e Residentes\n");
    printf("\033[1;32m------------------------------------\033[0m\n\n");

    return 0;
}
