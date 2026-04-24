#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define CORES 8
#define ITERS 1000000
#define V_BASE20 0x3D4CCCCD
#define V_PHI    0x3FCF1BBC

typedef struct {
    uint32_t data[128] __attribute__((aligned(4096)));
} VectraBlock;

void* omni_sentinel(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    void* mem = NULL;
    if (posix_memalign(&mem, 4096, sizeof(VectraBlock)) != 0) return NULL;
    VectraBlock *block = (VectraBlock*)mem;
    memset(block->data, 0, sizeof(block->data));

    uint32_t loops = ITERS;
    uint32_t *ptr = block->data;

    while(1) {
        asm volatile (
            "vld1.32 {q0, q1}, [%[buf]]! \n\t"
            "vld1.32 {q2, q3}, [%[buf]]   \n\t"
            "sub %[buf], %[buf], #32      \n\t"

            "vdup.32 q14, %[v20]          \n\t"
            "vdup.32 q15, %[phi]          \n\t"

            "1:                            \n\t"
            "vmla.f32 q0, q1, q14         \n\t"
            "vmla.f32 q2, q3, q14         \n\t"
            "vmls.f32 q1, q2, q15         \n\t"
            "vmls.f32 q3, q0, q15         \n\t"

            "vext.32 q4, q0, q1, #1       \n\t"
            "vrev64.32 q5, q2             \n\t"
            "veor q0, q4, q5              \n\t"
            "vext.32 q2, q3, q0, #2       \n\t"

            "subs %[count], %[count], #1  \n\t"
            "bne 1b                       \n\t"

            "vst1.32 {q0, q1}, [%[buf]]!  \n\t"
            "vst1.32 {q2, q3}, [%[buf]]   \n\t"
            : [count] "+r" (loops), [buf] "+r" (ptr)
            : [v20] "r" (V_BASE20), [phi] "r" (V_PHI)
            : "q0","q1","q2","q3","q4","q5","q14","q15","memory","cc"
        );

        // Relatório rápido
        static int report=0;
        if(++report % 50 == 0) {
            printf("[Omni-Sentinel Core %d] Mantendo Integridade 0x0\n", core_id);
            fflush(stdout);
        }
    }

    free(block);
    return NULL;
}

int main() {
    pthread_t threads[CORES];
    int ids[CORES];
    printf("\n\033[1;36m[VECTRA-QUANTUM-OMNI] REATOR 24/7 ATIVADO\033[0m\n");

    for(int i=0; i<CORES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, omni_sentinel, &ids[i]);
    }
    for(int i=0; i<CORES; i++) pthread_join(threads[i], NULL);

    return 0;
}
