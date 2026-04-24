#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define MAX_VARS 32
#define CLAUSES_FACTOR 4
#define STEPS 20000

typedef struct { int a,b,c; } Clause;

int VARS, CLAUSES;
Clause *F;

int rand_var(){
    int v = rand()%VARS + 1;
    return (rand()%2)? v : -v;
}

void gen_formula(){
    for(int i=0;i<CLAUSES;i++){
        F[i].a = rand_var();
        F[i].b = rand_var();
        F[i].c = rand_var();
    }
}

int eval_clause(Clause c, int s){
    int v[3]={c.a,c.b,c.c};
    for(int i=0;i<3;i++){
        int id = abs(v[i])-1;
        int val = (s>>id)&1;
        if(v[i]<0) val=!val;
        if(val) return 0;
    }
    return 1;
}

int energy(int s){
    int e=0;
    for(int i=0;i<CLAUSES;i++)
        e+=eval_clause(F[i],s);
    return e;
}

// mede maior energia durante tentativa de escape
int measure_barrier(int s){
    int base = energy(s);
    int maxE = base;

    for(int i=0;i<1000;i++){
        int v = rand()%VARS;
        s ^= (1<<v);
        int e = energy(s);
        if(e > maxE) maxE = e;
    }

    return maxE - base;
}

// aproxima condutância
double measure_phi(int trials){
    int escapes = 0;

    for(int i=0;i<trials;i++){
        int s = rand()%(1<<VARS);
        if(energy(s) <= 1){
            int v = rand()%VARS;
            int s2 = s ^ (1<<v);
            if(energy(s2) > 1) escapes++;
        }
    }

    return (double)escapes / trials;
}

void run(int n){
    VARS = n;
    CLAUSES = CLAUSES_FACTOR * n;

    F = malloc(sizeof(Clause)*CLAUSES);
    gen_formula();

    int samples = 100;
    double Bavg = 0;

    for(int i=0;i<samples;i++){
        int s = rand()%(1<<VARS);
        if(energy(s)==1){
            Bavg += measure_barrier(s);
        }
    }

    Bavg /= samples;

    double phi = measure_phi(5000);

    printf("\n=== n=%d ===\n",n);
    printf("Barrier ~ %.2f\n",Bavg);
    printf("Phi ~ %.6f\n",phi);

    free(F);
}

int main(){
    srand(time(NULL));

    run(10);
    run(20);
    run(30);
    run(35);
    run(40);

    return 0;
}
