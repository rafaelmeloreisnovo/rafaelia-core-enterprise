#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define MAX_VARS 40
#define CLAUSE_FACTOR 4
#define SAMPLES 20000
#define HIST_BINS 50

typedef struct { int a,b,c; } Clause;

int VARS, CLAUSES;
Clause *F;

// ================= RANDOM =================
int rand_var(){
    int v = rand()%VARS + 1;
    return (rand()%2)? v : -v;
}

// ================= FORMULA =================
void gen_formula(){
    for(int i=0;i<CLAUSES;i++){
        F[i].a = rand_var();
        F[i].b = rand_var();
        F[i].c = rand_var();
    }
}

// ================= ENERGY =================
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

// ================= BARRIER =================
int measure_barrier(int s){
    int base = energy(s);
    int maxE = base;

    for(int i=0;i<2000;i++){
        int v = rand()%VARS;
        s ^= (1<<v);
        int e = energy(s);
        if(e > maxE) maxE = e;
    }

    return maxE - base;
}

// ================= CONDUCTANCE =================
double measure_phi(int trials){
    int escapes = 0, valid=0;

    for(int i=0;i<trials;i++){
        int s = rand()%(1<<VARS);
        int e = energy(s);

        if(e <= CLAUSES * 0.05){
            valid++;
            int v = rand()%VARS;
            int s2 = s ^ (1<<v);

            if(energy(s2) > CLAUSES * 0.05)
                escapes++;
        }
    }

    if(valid==0) return 0.0;
    return (double)escapes / valid;
}

// ================= LANDSCAPE =================
void energy_histogram(){
    int hist[HIST_BINS]={0};

    for(int i=0;i<SAMPLES;i++){
        int s = rand()%(1<<VARS);
        int e = energy(s);

        int bin = (e * HIST_BINS) / (CLAUSES+1);
        if(bin>=HIST_BINS) bin=HIST_BINS-1;

        hist[bin]++;
    }

    printf("\n--- ENERGY LANDSCAPE ---\n");
    for(int i=0;i<HIST_BINS;i++){
        double x = (double)i / HIST_BINS;
        printf("%0.2f %d\n",x,hist[i]);
    }
}

// ================= RUN =================
void run(int n){
    VARS = n;
    CLAUSES = CLAUSE_FACTOR * n;

    F = malloc(sizeof(Clause)*CLAUSES);
    gen_formula();

    double Bsum = 0;
    int valid = 0;

    for(int i=0;i<SAMPLES;i++){
        int s = rand()%(1<<VARS);
        int e = energy(s);

        if(e <= CLAUSES * 0.05){
            Bsum += measure_barrier(s);
            valid++;
        }
    }

    double Bavg = (valid>0)? Bsum/valid : -1;
    double phi = measure_phi(5000);

    printf("\n=== n=%d ===\n",n);
    printf("valid states=%d\n",valid);
    printf("Barrier ~ %.4f\n",Bavg);
    printf("Phi ~ %.8f\n",phi);

    energy_histogram();

    free(F);
}

// ================= MAIN =================
int main(){
    srand(time(NULL));

    printf("=== VECTRA FULL LANDSCAPE ===\n");

    run(10);
    run(20);
    run(30);
    run(35);
    run(40);

    return 0;
}
