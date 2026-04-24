#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define MAX_VARS 40
#define TESTS 100
#define STEPS 30000

typedef struct { int a,b,c; } Clause;

int VARS, CLAUSES;
Clause *formula;

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

// ================= SOLVER =================

int solve(){

    int state = rand()%(1<<VARS);
    int e = total_energy(state);

    double T = 5.0;
    int stuck = 0;

    for(int step=0;step<STEPS;step++){

        int new_state;

        if(stuck > 200){
            int flips = 2 + rand()%3;
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

        if(e == 0) return step;

        if(stuck > 500){
            T *= 2.5;
            stuck = 0;
        }

        T *= 0.995;
        if(T < 1e-6) T = 1e-6;
    }

    return -1;
}

// ================= EXPERIMENTO =================

void run_scale(int vars){

    VARS = vars;
    CLAUSES = vars * 3;

    formula = malloc(sizeof(Clause)*CLAUSES);

    int success = 0;
    int total_steps = 0;

    for(int t=0;t<TESTS;t++){
        gen_formula();
        int s = solve();

        if(s >= 0){
            success++;
            total_steps += s;
        }
    }

    printf("\n=== VARS = %d ===\n",VARS);
    printf("sucesso: %d/%d\n",success,TESTS);

    if(success>0)
        printf("media passos: %d\n",total_steps/success);

    free(formula);
}

// ================= MAIN =================

int main(){
    srand(time(NULL));

    printf("=== VECTRA SCALE TEST ===\n");

    run_scale(10);
    run_scale(15);
    run_scale(20);
    run_scale(25);
    run_scale(30);
    run_scale(35);
    run_scale(40);

    return 0;
}
