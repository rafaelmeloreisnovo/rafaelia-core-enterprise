#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define N 8

// =======================
// 🧩 SAT OTIMIZADO
// =======================
typedef struct { int a,b,c; } Clause;

inline int eval_clause_fast(Clause cl, int a){
    int va=(a>>(abs(cl.a)-1))&1;
    int vb=(a>>(abs(cl.b)-1))&1;
    int vc=(a>>(abs(cl.c)-1))&1;

    if(cl.a<0) va=!va;
    if(cl.b<0) vb=!vb;
    if(cl.c<0) vc=!vc;

    return va|vb|vc;
}

int solve_sat_fast(Clause *c,int n,int vars){
    int max = 1<<vars;

    for(int a=0;a<max;a++){
        int ok=1;

        for(int i=0;i<n;i++){
            if(!eval_clause_fast(c[i],a)){
                ok=0;
                break; // early exit
            }
        }

        if(ok) return 1;
    }

    return 0;
}

// =======================
// 🌊 NAVIER HARDCORE
// =======================
void navier_fast(float *u){
    float t[N*N];

    for(int i=0;i<N;i++){
        int im = (i==0?N-1:i-1);
        int ip = (i==N-1?0:i+1);

        for(int j=0;j<N;j++){
            int jm = (j==0?N-1:j-1);
            int jp = (j==N-1?0:j+1);

            int idx = i*N+j;

            int up = im*N+j;
            int down = ip*N+j;
            int left = i*N+jm;
            int right = i*N+jp;

            float lap = u[up]+u[down]+u[left]+u[right]-4*u[idx];
            float adv = 0.25f*(u[right]-u[left] + u[down]-u[up]);

            t[idx]=u[idx]+0.1f*lap - 0.05f*adv;
        }
    }

    for(int i=0;i<N*N;i++) u[i]=t[i];
}

float energia_fast(float *u){
    float e=0;
    for(int i=0;i<N*N;i++) e+=u[i]*u[i];
    return e;
}

// =======================
// ⚛️ YANG-MILLS OTIMIZADO
// =======================
typedef struct { float w,x,y,z; } Q;

inline Q qmul(Q a,Q b){
    return (Q){
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    };
}

inline Q qconj(Q q){
    return (Q){q.w,-q.x,-q.y,-q.z};
}

inline float plaquette_fast(Q U1,Q U2,Q U3,Q U4){
    Q p=qmul(qmul(U1,U2),qmul(qconj(U3),qconj(U4)));
    return 1.0f - p.w;
}

// =======================
// 🔢 RIEMANN OTIMIZADO
// =======================
double zeta_fast(double s){
    double sum=0;
    for(int n=1;n<20000;n++)
        sum+=1.0/pow(n,s);
    return sum;
}

// =======================
// MAIN
// =======================
int main(){

    printf("=== SAT HARDCORE ===\n");
    Clause sat[]={{1,2,3},{-1,2,3}};
    Clause unsat[]={{1,1,1},{-1,-1,-1}};

    printf("SAT: %d\n",solve_sat_fast(sat,2,3));
    printf("UNSAT: %d\n",solve_sat_fast(unsat,2,1));

    printf("\n=== NAVIER HARDCORE ===\n");

    float u[N*N]={0};
    u[(N/2)*N + (N/2)] = 1.0f;

    for(int i=0;i<10;i++){
        navier_fast(u);
        printf("t=%d energia=%f\n",i,energia_fast(u));
    }

    printf("\n=== YANG-MILLS HARDCORE ===\n");

    Q I={1,0,0,0};
    Q pert={0.7f,0.7f,0,0};

    printf("flat: %f\n",plaquette_fast(I,I,I,I));
    printf("curvo: %f\n",plaquette_fast(pert,I,I,I));

    printf("\n=== RIEMANN HARDCORE ===\n");
    printf("zeta(2) ~ %f\n",zeta_fast(2.0));

    return 0;
}
