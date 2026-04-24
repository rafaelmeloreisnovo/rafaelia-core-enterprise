#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define VARS 20
#define CLAUSES 60
#define TESTS 200

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

// ================= SOLVER =================

int solve(int *final_energy){

    int state = rand()%(1<<VARS);
    int e = total_energy(state);

    double T = 5.0;

    for(int step=0;step<10000;step++){

        int v = rand()%VARS;
        int ns = state ^ (1<<v);

        int ne = total_energy(ns);
        int d = ne - e;

        if(d < 0 || ((double)rand()/RAND_MAX) < exp(-d/T)){
            state = ns;
            e = ne;
        }

        if(e==0){
            *final_energy = 0;
            return step;
        }

        T *= 0.995;
        if(T < 1e-6) T = 1e-6;
    }

    *final_energy = e;
    return -1;
}

// ================= DEBUG =================

void print_formula(){
    for(int i=0;i<CLAUSES;i++){
        printf("(%d %d %d) ",formula[i].a,formula[i].b,formula[i].c);
    }
    printf("\n");
}

// ================= MAIN =================

int main(){
    srand(time(NULL));

    int hard_cases = 0;

    for(int t=0;t<TESTS;t++){

        gen_formula();

        int final_e;
        int steps = solve(&final_e);

        if(steps < 0){
            hard_cases++;

            printf("\n=== HARD CASE %d ===\n",hard_cases);
            printf("energia final: %d\n",final_e);

            printf("formula:\n");
            print_formula();
        }
    }

    printf("\nTOTAL HARD CASES: %d/%d\n",hard_cases,TESTS);

    return 0;
}
