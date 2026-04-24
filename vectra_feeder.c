#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>

#define CORES 8
#define ITERS 1000000 

void* pulse_feeder(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    // FONTE DE BITS: Pegamos qualquer coisa do sistema como "combustível"
    int fd = open("/dev/urandom", O_RDONLY);
    uint32_t chaos_buffer[4]; 
    uint32_t loops = ITERS;

    for(uint32_t t=0; t < loops; t++) {
        read(fd, chaos_buffer, 16); // 128 bits de caos puro

        asm volatile (
            "vld1.32 {q0}, [%[input]] \n\t"  // Injeta o caos direto no q0
            "vdup.32 q14, %[force]    \n\t"  // Base 20
            
            // O VÓRTICE (Processamento por esmagamento)
            "vmla.f32 q1, q0, q14 \n\t"
            "vmla.f32 q2, q1, q14 \n\t"
            "vmla.f32 q3, q2, q14 \n\t"
            "vmla.f32 q0, q3, q14 \n\t" // Realimentação
            
            "vst1.32 {q0}, [%[input]] \n\t"  // Devolve o bit "processado"
            :
            : [input] "r" (chaos_buffer), [force] "r" (0x3D4CCCCD)
            : "q0","q1","q2","q3","q14","memory"
        );
    }
    close(fd);
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    printf("[VECTRA-FEEDER] TRANSFORMANDO CAOS EM GEOMETRIA...\n");

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, pulse_feeder, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    printf("PROCESSO CONCLUÍDO: BITS PURIFICADOS PELO VÓRTECE.\n");
    return 0;
}
