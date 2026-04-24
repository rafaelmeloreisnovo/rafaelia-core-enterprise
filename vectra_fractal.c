#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define DIM 6
#define CYCLES 56

// ================== SEQUÊNCIAS ==================
int fib_mod[]   = {0,0,0,1,1,2,3};      // 0001123
int trib_mod[]  = {0,1,1,2,3};          // 01123

#define FIB_LEN 7
#define TRIB_LEN 5

// ================== ESTADO ==================
typedef struct {
    double x[DIM];
    double prev[DIM];
    double prev2[DIM];
    double alpha, beta;
    double delta, C, H;
} State;

// ================== INIT ==================
void init(State *s) {
    memset(s, 0, sizeof(State));
    for(int i=0;i<DIM;i++)
        s->x[i] = 0.1 * (i+1);
}

// ================== PASSO ==================
void step(State *s, int t) {
    double new_x[DIM];
    double delta = 0;

    // 🔢 Sequências como fase
    s->alpha += fib_mod[t % FIB_LEN];
    s->beta  += trib_mod[t % TRIB_LEN];

    s->alpha = fmod(s->alpha, CYCLES);
    s->beta  = fmod(s->beta, CYCLES);

    // ⚖️ Paridade
    int parity = ((int)(s->alpha + s->beta)) % 2;

    for(int i=0;i<DIM;i++) {

        double d = s->x[i] - s->prev[i];
        double a = s->x[i] + s->prev[i] + s->prev2[i];
        double inv = fabs(s->x[i]) > 1e-6 ? 1.0/s->x[i] : 0.0;

        // 🌀 4 fractais
        double r1 = sin(s->prev[i]);
        double r2 = sin(s->prev[i] + s->prev2[i]);
        double r3 = sin(s->alpha);
        double r4 = sin(s->beta);
        double r = (r1 + r2 + r3 + r4) * 0.25;

        // 📐 Escala geométrica √3/2ⁿ
        double scale = pow(sqrt(3)/2.0, (t % 10));

        // ⚙️ Combinação
        new_x[i] = scale * (
            0.3*d +
            0.3*a +
            0.2*inv +
            0.2*r
        );

        // Paridade (tipo spin)
        if(!parity) new_x[i] *= -1.0;

        double diff = new_x[i] - s->x[i];
        delta += diff * diff;
    }

    delta = sqrt(delta);

    // histórico
    for(int i=0;i<DIM;i++){
        s->prev2[i] = s->prev[i];
        s->prev[i]  = s->x[i];
        s->x[i]     = new_x[i];
    }

    // métricas
    s->delta = delta;
    s->C = 1.0 / (1.0 + delta);
    s->H = log(1.0 + delta);
}

// ================== HASH ==================
uint64_t hash_state(State *s) {
    uint64_t h = 0;
    for(int i=0;i<DIM;i++){
        int64_t v = (int64_t)(s->x[i] * 1000);
        h ^= (v + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
    }
    return h;
}

// ================== MAIN ==================
int main(int argc, char **argv) {

    int steps = 500000;
    int transient = 10000;

    if(argc > 1) steps = atoi(argv[1]);
    if(argc > 2) transient = atoi(argv[2]);

    State s;
    init(&s);

    printf("=== VECTRA FRACTAL-PARIDADE ===\n");

    // 🔥 transiente
    for(int i=0;i<transient;i++)
        step(&s, i);

    uint64_t seen[10000];
    int seen_step[10000];
    int count = 0;

    for(int t=0;t<steps;t++) {

        step(&s, t);

        uint64_t h = hash_state(&s);

        for(int i=0;i<count;i++){
            if(seen[i] == h){
                int cycle = t - seen_step[i];

                printf("\n🔥 CICLO DETECTADO\n");
                printf("Inicio: %d\n", seen_step[i]);
                printf("Periodo: %d\n", cycle);
                printf("C: %.5f | H: %.5f\n", s.C, s.H);

                return 0;
            }
        }

        if(count < 10000){
            seen[count] = h;
            seen_step[count] = t;
            count++;
        }

        if((t+1) % 100000 == 0)
            printf("Passo %d...\n", t+1);
    }

    printf("\n🌪️ Nenhum ciclo detectado (caos ou quasi-periodico)\n");
    printf("C: %.5f | H: %.5f\n", s.C, s.H);

    return 0;
}
