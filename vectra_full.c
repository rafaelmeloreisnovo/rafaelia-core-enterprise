#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define N 6

// =======================
// 🧩 3-SAT (backtracking correto)
// =======================
typedef struct { int a,b,c; } Clause;

int eval_clause(Clause cl, int a){
    int va=(a>>(abs(cl.a)-1))&1;
    int vb=(a>>(abs(cl.b)-1))&1;
    int vc=(a>>(abs(cl.c)-1))&1;
    if(cl.a<0) va=!va;
    if(cl.b<0) vb=!vb;
    if(cl.c<0) vc=!vc;
    return va||vb||vc;
}

int solve_sat_bt(Clause *c,int n,int vars,int idx,int assign){
    if(idx==vars){
        for(int i=0;i<n;i++)
            if(!eval_clause(c[i],assign))
                return 0;
        return 1;
    }

    if(solve_sat_bt(c,n,vars,idx+1,assign))
        return 1;

    if(solve_sat_bt(c,n,vars,idx+1,assign|(1<<idx)))
        return 1;

    return 0;
}

// =======================
// 🌊 Navier-Stokes (difusão + advecção)
// =======================
void navier(float *u){
    float t[N*N];

    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){

            int idx=i*N+j;
            int up=((i-1+N)%N)*N+j;
            int down=((i+1)%N)*N+j;
            int left=i*N+(j-1+N)%N;
            int right=i*N+(j+1)%N;

            float lap = u[up]+u[down]+u[left]+u[right]-4*u[idx];
            float adv = 0.25f*(u[right]-u[left] + u[down]-u[up]);

            t[idx]=u[idx]+0.1f*lap - 0.05f*adv;
        }
    }

    for(int i=0;i<N*N;i++) u[i]=t[i];
}

float energia(float *u){
    float e=0;
    for(int i=0;i<N*N;i++) e+=u[i]*u[i];
    return e;
}

// =======================
// ⚛️ Yang-Mills (SU(2))
// =======================
typedef struct { float w,x,y,z; } Q;

Q qmul(Q a,Q b){
    Q r;
    r.w=a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
    r.x=a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
    r.y=a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
    r.z=a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
    return r;
}

Q qconj(Q q){
    Q r={q.w,-q.x,-q.y,-q.z};
    return r;
}

float plaquette(Q U1,Q U2,Q U3,Q U4){
    Q p=qmul(qmul(U1,U2),qmul(qconj(U3),qconj(U4)));
    return 1.0f - p.w;
}

// =======================
// 🔢 Riemann (zeta)
// =======================
double zeta(double s){
    double sum=0;
    for(int n=1;n<10000;n++)
        sum+=1.0/pow(n,s);
    return sum;
}

// =======================
// 🧬 BSD (toy)
// =======================
double elliptic(double x){
    return x*x*x - x + 1;
}

// =======================
// 🧠 Hodge (placeholder)
// =======================
int hodge_check(){
    return 1;
}

// =======================
// ⚛️ Mass Gap
// =======================
float mass_gap(float e1,float e2){
    return fabs(e1-e2);
}

// =======================
// MAIN
// =======================
int main(){

    printf("=== P vs NP (SAT) ===\n");
    Clause sat[]={{1,2,3},{-1,2,3}};
    Clause unsat[]={{1,1,1},{-1,-1,-1}};

    printf("SAT: %d\n",solve_sat_bt(sat,2,3,0,0));
    printf("UNSAT: %d\n",solve_sat_bt(unsat,2,1,0,0));

    printf("\n=== Navier-Stokes ===\n");
    float u[N*N]={0};
    u[10]=1.0f;

    for(int i=0;i<8;i++){
        navier(u);
        printf("t=%d energia=%f\n",i,energia(u));
    }

    printf("\n=== Yang-Mills SU(2) ===\n");

    Q I={1,0,0,0};
    Q pert={0.7,0.7,0,0};

    printf("flat: %f\n",plaquette(I,I,I,I));
    printf("curvo: %f\n",plaquette(pert,I,I,I));

    printf("\n=== Riemann ===\n");
    printf("zeta(2) ~ %f\n",zeta(2.0));

    printf("\n=== BSD ===\n");
    printf("elliptic(2) = %f\n",elliptic(2));

    printf("\n=== Hodge ===\n");
    printf("check = %d\n",hodge_check());

    printf("\n=== Mass Gap ===\n");
    printf("gap = %f\n",mass_gap(0.3f,0.6f));

    return 0;
}
