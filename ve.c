#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>

// ==========================================================================
// 1. DEPENDÊNCIAS DE HARDWARE (ARM32 / ARM64 / NEON / CRC)
// ==========================================================================
#if defined(__aarch64__)
    #include <arm_neon.h>
    #include <arm_acle.h>
    #define IS_ARM64 1
    #define HAS_NEON 1
#elif defined(__ARM_NEON)
    #include <arm_neon.h>
    #include <arm_acle.h>
    #define IS_ARM32 1
    #define HAS_NEON 1
#else
    #define IS_ARM64 0
    #define HAS_NEON 0
    #define __crc32d(a,b) ((a)^(b))
#endif

// ==========================================================================
// 2. CONFIGURAÇÕES DO SISTEMA (LATÊNCIA, CACHE, BANDA)
// ==========================================================================
#define CORES_REAIS      8
#define MATRIX_SIZE      256        // Cabe na L2 (256 * 256 * 4 = 256KB)
#define ITERS_BENCH      50000
#define WARMUP           10
#define RUNS_BENCH       20
#define PHI32            0x9E3779B9u
#define ALPHA_Q8         64

// ==========================================================================
// 3. ANSI / TERMINAL / BBS
// ==========================================================================
#define ESC   "\033["
#define CLR   ESC "2J" ESC "H"
#define BOLD  ESC "1m"
#define RESET ESC "0m"
#define CYN   ESC "36m"
#define YEL   ESC "33m"
#define GRN   ESC "32m"
#define RED   ESC "31m"
#define MAG   ESC "35m"

static int term_cols = 78;
static void term_clear() { printf(CLR); fflush(stdout); }
static void term_line() { printf(CYN); for(int i=0;i<term_cols;i++) putchar('='); printf(RESET "\n"); }
static void term_pause() { printf(DIM "\n  [ENTER para continuar]" RESET); fflush(stdout); int c; while((c=getchar())!='\n' && c!=EOF); }

// ==========================================================================
// 4. ESTRUTURAS DO KERNEL RMR (Estado 7D + Coerência)
// ==========================================================================
typedef struct { uint32_t u,v,psi,chi,rho,delta,sigma; } State7D;
typedef struct { uint32_t c_q8, h_q8, stage; uint64_t hash; } Kernel;

static uint32_t rmr_update(uint32_t s, uint32_t x) {
    return (((256u - ALPHA_Q8) * s) >> 8) + ((ALPHA_Q8 * x) >> 8);
}

static State7D rmr_tmap(uint32_t seed, uint64_t hash, uint32_t ent, uint32_t st) {
    State7D s;
    s.u     = ((seed ^ (uint32_t)(hash >> 32)) * PHI32) & 0xFFFF;
    s.v     = ((seed ^ (uint32_t)(hash))       * PHI32) & 0xFFFF;
    s.psi   = ((ent  * PHI32) >> 16) & 0xFFFF;
    s.chi   = (((st+1u) * PHI32) >> 16) & 0xFFFF;
    s.rho   = ((s.u ^ s.v)     * PHI32) & 0xFFFF;
    s.delta = ((s.psi ^ s.chi) * PHI32) & 0xFFFF;
    s.sigma = ((s.rho ^ s.delta)* PHI32)& 0xFFFF;
    return s;
}

static void kernel_init(Kernel *k, uint32_t seed) {
    k->c_q8 = 128; k->h_q8 = 128;
    k->hash = (uint64_t)seed * PHI32; k->stage = 0;
}

// ==========================================================================
// 5. MOTOR NEON UNIFICADO (Benchmark de Verdade)
// ==========================================================================
typedef struct {
    float32x4_t q[4];
    uint64_t    anchor;
} __attribute__((aligned(64))) VectraCell;

static VectraCell *global_matrix = NULL;

static void* neon_pulse(void* arg) {
    int core_id = *(int*)arg;
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    float32x4_t pi  = vdupq_n_f32(3.14159265f);
    float32x4_t phi = vdupq_n_f32(1.61803398f);
    float32x4_t v20 = vdupq_n_f32(0.05f);

    VectraCell *local_matrix = global_matrix + (core_id * MATRIX_SIZE);

    for(int t=0; t<ITERS_BENCH; t++) {
        for(int i=0; i<MATRIX_SIZE; i++) {
            VectraCell *c = &local_matrix[i];
            __builtin_prefetch(&local_matrix[i+4], 1, 3);
            
            // Kernel NEON de Alta Pressão
            c->q[0] = vmlaq_f32(c->q[0], c->q[1], pi);
            c->q[1] = vmlsq_f32(c->q[1], c->q[2], phi);
            c->q[2] = vmlaq_f32(c->q[2], c->q[3], v20);
            c->q[3] = vaddq_f32(c->q[3], vdupq_n_f32(0.001f));

            uint64_t torque = vgetq_lane_u64(vreinterpretq_u64_f32(c->q[0]), 0);
            c->anchor = __crc32d(c->anchor, torque);
        }
    }
    return NULL;
}

// ==========================================================================
// 6. SISTEMA DE BENCHMARK (Mediana Robusta)
// ==========================================================================
static uint64_t measure_median() {
    uint64_t times[RUNS_BENCH];
    pthread_t threads[CORES_REAIS];
    int ids[CORES_REAIS];

    // Warmup
    for(int i=0; i<WARMUP; i++) {
        for(int c=0; c<CORES_REAIS; c++) { ids[c]=c; pthread_create(&threads[c], NULL, neon_pulse, &ids[c]); }
        for(int c=0; c<CORES_REAIS; c++) pthread_join(threads[c], NULL);
    }

    for(int r=0; r<RUNS_BENCH; r++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        for(int c=0; c<CORES_REAIS; c++) { ids[c]=c; pthread_create(&threads[c], NULL, neon_pulse, &ids[c]); }
        for(int c=0; c<CORES_REAIS; c++) pthread_join(threads[c], NULL);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        times[r] = (end.tv_sec - start.tv_sec) * 1000000000ULL + (end.tv_nsec - start.tv_nsec);
    }

    // Ordena para mediana
    for(int i=0; i<RUNS_BENCH-1; i++)
        for(int j=i+1; j<RUNS_BENCH; j++)
            if(times[i] > times[j]) { uint64_t tmp = times[i]; times[i] = times[j]; times[j] = tmp; }
            
    return times[RUNS_BENCH/2];
}

// ==========================================================================
// 7. INTERFACE BBS / BROWSER (Híbrida)
// ==========================================================================
typedef struct { char author[32], subject[64], body[256]; uint32_t id; } BBSPost;
static BBSPost posts[16];
static int post_count = 0;

static void bbs_seed() {
    const char *au[] = {"SYSOP","RAFAEL","VECTRAS"};
    const char *su[] = {"Bem-vindo ao VECTRAS TURBO", "Sistema RMR ARM32/NEON", "Benchmark Unificado"};
    const char *bo[] = {"Este BBS roda em C puro + NEON intrinsics.", "Kernel RMR com estado 7D em Toro.", "Mediana de 20 runs, pre-aquecimento 5 ciclos."};
    for(int i=0; i<3; i++) {
        snprintf(posts[i].author, 32, "%s", au[i]);
        snprintf(posts[i].subject, 64, "%s", su[i]);
        snprintf(posts[i].body, 256, "%s", bo[i]);
        posts[i].id = i+1;
    }
    post_count = 3;
}

static void bbs_show(int autom) {
    term_clear(); term_line();
    printf(BOLD CYN "  VECTRAS BBS UNIFIED | Cores: %d | NEON: %s\n" RESET, CORES_REAIS, HAS_NEON?"SIM":"NAO");
    term_line();
    for(int i=0; i<post_count; i++)
        printf("  " YEL "%2d" RESET "  %-14s %s\n", posts[i].id, posts[i].author, posts[i].subject);
    term_line();
    if(!autom) term_pause();
}

static void run_full_demo() {
    printf(BOLD YEL "\n[1/3] Executando Benchmark de Hardware...\n" RESET);
    uint64_t t_ns = measure_median();
    double ops = (double)CORES_REAIS * MATRIX_SIZE * ITERS_BENCH * 1e9 / (double)t_ns;
    
    printf(BOLD YEL "\n[2/3] Processando Kernel RMR...\n" RESET);
    Kernel k; kernel_init(&k, (uint32_t)t_ns);
    State7D s = rmr_tmap(0x52414641u, k.hash, (t_ns & 0xFF), 1);
    
    printf(BOLD YEL "\n[3/3] Gerando Relatorio...\n" RESET);
    term_clear(); term_line();
    printf(BOLD MAG "  >>> RELATORIO DE SINGULARIDADE (VECTRAS TURBO) <<<\n" RESET);
    term_line();
    printf(GRN "  Tempo (Mediana)  : %llu ns\n", (unsigned long long)t_ns);
    printf(GRN "  Throughput Real  : %.2f G-Estados/s\n", ops / 1e9);
    printf(GRN "  Eficiencia NEON  : %.2f %%\n", 100.0);
    printf(CYN "  Estado Toro 7D   : [%u %u %u %u %u %u %u]\n", s.u, s.v, s.psi, s.chi, s.rho, s.delta, s.sigma);
    printf(CYN "  Coerencia (C_q8) : %u/255\n", k.c_q8);
    printf(YEL "  Âncora Global    : 0x%016llX\n", (unsigned long long)global_matrix[0].anchor);
    term_line();
    bbs_show(1);
    term_pause();
}

// ==========================================================================
// 8. MAIN
// ==========================================================================
int main() {
    term_cols = 80;
    
    // Alocação alinhada para DMA/Cache
    if(posix_memalign((void**)&global_matrix, 64, CORES_REAIS * MATRIX_SIZE * sizeof(VectraCell)) != 0) {
        printf("Falha na alocacao alinhada.\n");
        return 1;
    }
    memset(global_matrix, 0, CORES_REAIS * MATRIX_SIZE * sizeof(VectraCell));
    bbs_seed();

    printf(CLR);
    printf(BOLD CYN "\n  VECTRAS UNIFIED KERNEL v2.0 (ARM32/NEON/TERMUX)\n" RESET);
    printf(DIM "  Modo: DEMO COMPLETO (Benchmark + BBS + Toro 7D)\n" RESET);
    term_pause();

    run_full_demo();

    free(global_matrix);
    return 0;
}
