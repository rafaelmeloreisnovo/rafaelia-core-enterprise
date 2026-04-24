#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define VARS 20
#define CLAUSES 60
#define STEPS 20000

typedef struct { int a,b,c; } Clause;
Clause formula[CLAUSES];

// ================= UTIL =================

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

// ================= ENERGIA =================

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

// ================= HARDCORE SOLVER =================

int solve(){

    int state = rand()%(1<<VARS);
    int e = total_energy(state);

    double T = 5.0;
    int stuck = 0;

    for(int step=0;step<STEPS;step++){

        int new_state;

        // 🔥 MULTI-FLIP quando travado
        if(stuck > 200){
            int flips = 2 + rand()%3; // 2 a 4 flips
            new_state = state;

            for(int k=0;k<flips;k++){
                int v = rand()%VARS;
                new_state ^= (1<<v);
            }
        } else {
            int v = rand()%VARS;
            new_state = state ^ (1<<v);
        }

        int new_e = total_energy(new_state);
        int delta = new_e - e;

        if(delta < 0 || ((double)rand()/RAND_MAX) < exp(-delta/T)){
            state = new_state;

            if(new_e < e) stuck = 0;
            else stuck++;

            e = new_e;
        } else {
            stuck++;
        }

        // ✔ solução
        if(e == 0){
            printf("✔ resolvido em %d passos\n",step);
            return 1;
        }

        // 🔥 REHEATING (CRÍTICO)
        if(stuck > 500){
            T *= 2.5;
            stuck = 0;
        }

        // resfriamento normal
        T *= 0.995;
        if(T < 1e-6) T = 1e-6;
    }

    printf("✖ falhou, energia final: %d\n",e);
    return 0;
}

// ================= MAIN =================

int main(){
    srand(time(NULL));

    int success = 0;

    for(int i=0;i<50;i++){
        gen_formula();
        success += solve();
    }

    printf("\nSUCESSO: %d/50\n",success);

    return 0;
}
