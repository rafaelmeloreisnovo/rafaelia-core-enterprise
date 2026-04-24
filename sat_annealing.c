#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define VARS 20
#define CLAUSES 80
#define STEPS 20000

typedef struct { int a,b,c; } Clause;
Clause formula[CLAUSES];

// =======================
// 🔧 Utils
// =======================
int rand_var(){
    int v = rand()%VARS + 1;
    return (rand()%2)? v : -v;
}

void gen_formula(){
    for(int i=0;i<CLAUSES;i++){
        formula[i].a = rand_var();
        formula[i].b = rand_var();
        formula[i].c = rand_var();
    }
}

// =======================
// ⚡ Energia
// =======================
int clause_energy(Clause cl, int assign){
    int vars[3]={cl.a,cl.b,cl.c};

    for(int i=0;i<3;i++){
        int v=vars[i];
        int id=abs(v)-1;
        int val=(assign>>id)&1;
        if(v<0) val=!val;
        if(val) return 0;
    }
    return 1;
}

int total_energy(int assign){
    int e=0;
    for(int i=0;i<CLAUSES;i++)
        e+=clause_energy(formula[i],assign);
    return e;
}

// =======================
// 🔥 SIMULATED ANNEALING
// =======================
int solve_annealing(){

    int state = rand()%(1<<VARS);
    int best = state;

    int e = total_energy(state);
    int best_e = e;

    double T = 5.0;          // temperatura inicial
    double alpha = 0.995;    // resfriamento

    for(int step=0; step<STEPS; step++){

        // escolhe variável aleatória
        int v = rand()%VARS;
        int new_state = state ^ (1<<v);

        int new_e = total_energy(new_state);
        int delta = new_e - e;

        // regra de aceitação
        if(delta < 0){
            state = new_state;
            e = new_e;
        } else {
            double prob = exp(-(double)delta / T);
            double r = (double)rand() / (double)RAND_MAX;

            if(r < prob){
                state = new_state;
                e = new_e;
            }
        }

        // melhor global
        if(e < best_e){
            best = state;
            best_e = e;
        }

        // sucesso total
        if(best_e == 0){
            printf("✔ solução encontrada no passo %d\n", step);
            return 1;
        }

        // resfriamento
        T *= alpha;

        // evita T=0
        if(T < 1e-6) T = 1e-6;
    }

    printf("✖ melhor energia encontrada: %d\n", best_e);
    return 0;
}

// =======================
// MAIN
// =======================
int main(){
    srand(time(NULL));

    printf("=== SAT ANNEALING ===\n");

    gen_formula();

    int ok = solve_annealing();

    printf("resultado: %d\n", ok);

    return 0;
}
