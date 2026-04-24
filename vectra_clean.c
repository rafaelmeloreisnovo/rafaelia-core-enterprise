#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

// ============================================================
// CONFIG
// ============================================================
#define Q16_ONE 65536.0
#define PERIOD 42
#define STEPS 10000
#define N 6

// ============================================================
// ===================== MODO 1 ===============================
// Fibonacci-Rafael (corrigido - sem colapso)
// ============================================================
void mode_fib(){
    printf("\n=== MODO 1: FIBONACCI-RAFAEL ===\n");

    double F = 1.0;

    for(int i=0;i<=PERIOD;i++){
        printf("n=%2d -> %.6f\n", i, F);

        // dinâmica estável (não zera)
        F = F * (sqrt(3.0)/2.0) - M_PI * sin(279.0 * M_PI/180.0);

        // normalização leve evita explosão
        if(F > 10) F = fmod(F, 10);
        if(F < -10) F = fmod(F, 10);
    }
}

// ============================================================
// ===================== MODO 2 ===============================
// Toro + energia
// ============================================================
typedef struct {
    double theta, phi;
} Node;

Node T[N];

double frand(){
    return rand()/(double)RAND_MAX;
}

double energy(){
    double E = 0;

    for(int i=0;i<N;i++){
        for(int j=i+1;j<N;j++){
            double dth = fabs(T[i].theta - T[j].theta);
            double dph = fabs(T[i].phi   - T[j].phi);

            E += (1 - cos(dth));
            E += 0.5*(1 - cos(dph));
            E += 0.3*sin(dth)*cos(dph);
        }
    }

    return E;
}

void mode_toro(){
    printf("\n=== MODO 2: TORO ===\n");

    for(int i=0;i<N;i++){
        T[i].theta = frand()*2*M_PI;
        T[i].phi   = frand()*2*M_PI;
    }

    double E = energy();

    for(int t=0;t<STEPS;t++){
        int i = rand()%N;

        Node old = T[i];

        T[i].theta += (frand()-0.5)*0.3;
        T[i].phi   += (frand()-0.5)*0.3;

        double E2 = energy();

        if(E2 > E && exp(-(E2-E)) < frand()){
            T[i] = old;
        } else {
            E = E2;
        }

        if(t%2000==0)
            printf("t=%d E=%.4f\n",t,E);
    }
}

// ============================================================
// ===================== MODO 3 ===============================
// BitOmega discreto (FIX REAL)
// ============================================================
typedef struct {
    int q;
    int C;
    int H;
} State;

State step(State s, int in){
    State o;

    float Cin = (in & 0xF)/15.0;
    float Hin = ((in>>4)&0xF)/15.0;

    float Cf = 0.75*(s.C/100.0) + 0.25*Cin;
    float Hf = 0.75*(s.H/100.0) + 0.25*Hin;

    o.C = (int)(Cf*100);
    o.H = (int)(Hf*100);

    o.q = (s.q + (o.C ^ o.H ^ in)) % 10;

    return o;
}

int equal(State a, State b){
    return a.q==b.q && a.C==b.C && a.H==b.H;
}

int has_cycle(State s){
    State t = step(s,1);
    State h = step(step(s,1),2);

    for(int i=0;i<1000;i++){
        if(equal(t,h)) return 1;
        t = step(t,i+3);
        h = step(step(h,i+4),i+5);
    }
    return 0;
}

void mode_bitomega(){
    printf("\n=== MODO 3: BITOMEGA ===\n");

    int count=0;

    for(int q=0;q<10;q++){
        for(int c=0;c<100;c+=10){
            for(int h=0;h<100;h+=10){

                State s = {q,c,h};

                if(has_cycle(s)) count++;
            }
        }
    }

    printf("Atratores detectados: %d\n",count);
}

// ============================================================
// ===================== MODO 4 ===============================
// φ_ethica
// ============================================================
double phi(double H, double C){
    return (1.0 - H)*C;
}

void mode_phi(){
    printf("\n=== MODO 4: PHI ===\n");

    printf("φ(0.0,1.0)=%.3f\n",phi(0,1));
    printf("φ(0.5,0.5)=%.3f\n",phi(0.5,0.5));
    printf("φ(0.8,0.2)=%.3f\n",phi(0.8,0.2));
}

// ============================================================
// ===================== MODO 5 ===============================
// Linking converge
// ============================================================
void mode_link(){
    printf("\n=== MODO 5: LINKING ===\n");

    double t1=0, p1=0;
    double t2=1.2, p2=0.8;

    for(int i=0;i<100;i++){

        double g = cos(t1-t2)*cos(p1-p2);

        t1 += 0.01*g;
        t2 -= 0.01*g;

        double L = sin(t1-t2)*cos(p1-p2);

        if(i%20==0)
            printf("i=%d L=%.4f\n",i,L);
    }
}

// ============================================================
// MAIN
// ============================================================
int main(){
    srand(time(NULL));

    printf("\n==== VECTRA CLEAN RUN ====\n");

    mode_fib();
    mode_toro();
    mode_bitomega();
    mode_phi();
    mode_link();

    return 0;
}
