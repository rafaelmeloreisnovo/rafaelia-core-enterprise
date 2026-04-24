#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 6
#define VARS 16
#define CLAUSES 40
#define STEPS 10000

// =======================
// 🧩 SAT (ANNEALING)
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
    return 1;
}

int total_energy(int assign){
    int e=0;
    for(int i=0;i<CLAUSES;i++)
        e+=clause_energy(formula[i],assign);
    return e;
}

int solve_sat(){
    int state = rand()%(1<<VARS);
    int e = total_energy(state);

    double T = 5.0;
    double alpha = 0.995;

    for(int step=0;step<STEPS;step++){

        int v = rand()%VARS;
        int new_state = state ^ (1<<v);
        int new_e = total_energy(new_state);
        int delta = new_e - e;

        if(delta < 0 || ((double)rand()/RAND_MAX) < exp(-delta/T)){
            state = new_state;
            e = new_e;
        }

        if(e==0){
            printf("SAT resolvido em %d passos\n",step);
            return 1;
        }

        T *= alpha;
        if(T < 1e-6) T = 1e-6;
    }

    printf("SAT energia final: %d\n",e);
    return 0;
}

// =======================
// 🌊 NAVIER
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

float navier_energy(){
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

            float d1 = field[idx]-field[right];
            float d2 = field[idx]-field[down];

            e += d1*d1 + d2*d2;
        }
    }

    return e;
}

// =======================
// ⚛️ MASS GAP
// =======================
float mass_gap(){
    float min=1e9, max=-1e9;

    for(int i=0;i<N*N;i++){
        if(field[i]<min) min=field[i];
        if(field[i]>max) max=field[i];
    }

    return fabs(max-min);
}

// =======================
// 🔢 RIEMANN
// =======================
double zeta(double s){
    double sum=0;
    for(int n=1;n<20000;n++)
        sum+=1.0/pow(n,s);
    return sum;
}

// =======================
// 🧬 BSD
// =======================
double elliptic(double x){
    return x*x*x - x + 1;
}

// =======================
// 🧠 HODGE (toy)
// =======================
int hodge_check(){
    return 1;
}

// =======================
// MAIN
// =======================
int main(){
    srand(time(NULL));

    printf("=== SAT ===\n");
    gen_formula();
    printf("resultado: %d\n",solve_sat());

    printf("\n=== NAVIER ===\n");
    for(int i=0;i<N*N;i++) grid[i]=0;
    grid[N*N/2]=1.0;

    for(int i=0;i<8;i++){
        navier();
        printf("t=%d E=%f\n",i,navier_energy());
    }

    printf("\n=== YANG-MILLS ===\n");
    for(int i=0;i<N*N;i++)
        field[i]=(float)rand()/(float)RAND_MAX;

    printf("energia=%f\n",ym_energy());

    printf("\n=== MASS GAP ===\n");
    printf("gap=%f\n",mass_gap());

    printf("\n=== RIEMANN ===\n");
    printf("zeta(2)=%f\n",zeta(2.0));

    printf("\n=== BSD ===\n");
    printf("elliptic(2)=%f\n",elliptic(2));

    printf("\n=== HODGE ===\n");
    printf("check=%d\n",hodge_check());

    return 0;
}
