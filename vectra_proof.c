#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define SAMPLES 10000
#define CLAUSE_FACTOR 4

typedef struct { int a,b,c; } Clause;

int VARS, CLAUSES;
Clause *F;

int rand_var(){
    int v = rand() % VARS + 1;
    return (rand() % 2) ? v : -v;
}

void gen_formula(){
    for(int i = 0; i < CLAUSES; i++){
        F[i].a = rand_var();
        F[i].b = rand_var();
        F[i].c = rand_var();
    }
}

int eval_clause(Clause c, int s){
    int v[3] = {c.a, c.b, c.c};
    for(int i = 0; i < 3; i++){
        int id = abs(v[i]) - 1;
        int val = (s >> id) & 1;
        if(v[i] < 0) val = !val;
        if(val) return 0;
    }
    return 1;
}

int energy(int s){
    int e = 0;
    for(int i = 0; i < CLAUSES; i++)
        e += eval_clause(F[i], s);
    return e;
}

int measure_barrier(int s){
    int base = energy(s);
    int maxE = base;
    // caminhada aleatória de 1500 flips para explorar a vizinhança
    for(int i = 0; i < 1500; i++){
        int v = rand() % VARS;
        s ^= (1 << v);
        int e = energy(s);
        if(e > maxE) maxE = e;
    }
    return maxE - base;
}

void run(int n){
    VARS = n;
    CLAUSES = CLAUSE_FACTOR * n;
    F = malloc(sizeof(Clause) * CLAUSES);
    gen_formula();

    double Bsum = 0.0;
    int valid = 0;

    for(int i = 0; i < SAMPLES; i++){
        int s = rand() % (1 << VARS);
        int e = energy(s);
        // considera estados com energia muito baixa (≤ 5% das cláusulas)
        if(e <= CLAUSES * 0.05){
            Bsum += measure_barrier(s);
            valid++;
        }
    }

    double Bavg = (valid > 0) ? Bsum / valid : 0.0;
    printf("%d %.4f %d\n", n, Bavg, valid);

    free(F);
}

int main(){
    srand(time(NULL));
    printf("n B(n) valid_samples\n");
    for(int n = 10; n <= 40; n += 5)
        run(n);
    return 0;
}
