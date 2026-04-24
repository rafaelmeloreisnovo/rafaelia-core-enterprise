#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 6
#define VARS 10
#define CLAUSES 20

// =======================
// 🧩 SAT via ENERGIA (ataque real)
// =======================

typedef struct { int a,b,c; } Clause;

Clause formula[CLAUSES];

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

int clause_energy(Clause cl, int assign){
    int vars[3]={cl.a,cl.b,cl.c};

    for(int i=0;i<3;i++){
        int v=vars[i];
        int id=abs(v)-1;
        int val=(assign>>id)&1;
        if(v<0) val=!val;
        if(val) return 0;
    }
    return 1; // penalidade
}

int total_energy(int assign){
    int e=0;
    for(int i=0;i<CLAUSES;i++)
        e+=clause_energy(formula[i],assign);
    return e;
}

int solve_sat_energy(){
    int state = rand()%(1<<VARS);

    for(int step=0;step<5000;step++){

        int best = state;
        int best_e = total_energy(state);

        for(int v=0;v<VARS;v++){
            int flip = state ^ (1<<v);
            int e = total_energy(flip);

            if(e < best_e){
                best = flip;
                best_e = e;
            }
        }

        state = best;

        if(best_e==0) return 1;
    }

    return 0;
}

// =======================
// 🌊 NAVIER (medir estabilidade)
// =======================

float grid[N*N];

void navier(){
    float t[N*N];

    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){

            int idx=i*N+j;

            int up=((i-1+N)%N)*N+j;
            int down=((i+1)%N)*N+j;
            int left=i*N+(j-1+N)%N;
            int right=i*N+(j+1)%N;

            float lap = grid[up]+grid[down]+grid[left]+grid[right]-4*grid[idx];

            t[idx]=grid[idx]+0.1f*lap;
        }
    }

    for(int i=0;i<N*N;i++) grid[i]=t[i];
}

float energy_fluid(){
    float e=0;
    for(int i=0;i<N*N;i++) e+=grid[i]*grid[i];
    return e;
}

// =======================
// ⚛️ YANG-MILLS GRID
// =======================

float field[N*N];

float ym_energy(){
    float e=0;

    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){
            int idx=i*N+j;
            int right=i*N+(j+1)%N;
            int down=((i+1)%N)*N+j;

            float diff1 = field[idx]-field[right];
            float diff2 = field[idx]-field[down];

            e += diff1*diff1 + diff2*diff2;
        }
    }

    return e;
}

// =======================
// ⚛️ MASS GAP ESTIMATIVO
// =======================

float mass_gap_estimate(){
    float min=1e9, max=0;

    for(int i=0;i<N*N;i++){
        if(field[i]<min) min=field[i];
        if(field[i]>max) max=field[i];
    }

    return fabs(max-min);
}

// =======================
// 🔢 RIEMANN (linha crítica)
// =======================

double zeta(double s){
    double sum=0;
    for(int n=1;n<20000;n++)
        sum+=1.0/pow(n,s);
    return sum;
}

// =======================
// MAIN
// =======================

int main(){
    srand(time(NULL));

    printf("=== SAT (energia dinâmica) ===\n");
    gen_formula();
    printf("SAT found: %d\n",solve_sat_energy());

    printf("\n=== NAVIER (estabilidade) ===\n");
    for(int i=0;i<N*N;i++) grid[i]=0;
    grid[N*N/2]=1.0f;

    for(int i=0;i<10;i++){
        navier();
        printf("t=%d E=%f\n",i,energy_fluid());
    }

    printf("\n=== YANG-MILLS GRID ===\n");
    for(int i=0;i<N*N;i++)
        field[i]=(float)rand()/RAND_MAX;

    printf("energy=%f\n",ym_energy());

    printf("\n=== MASS GAP ===\n");
    printf("gap=%f\n",mass_gap_estimate());

    printf("\n=== RIEMANN ===\n");
    printf("zeta(2)=%f\n",zeta(2.0));

    return 0;
}
