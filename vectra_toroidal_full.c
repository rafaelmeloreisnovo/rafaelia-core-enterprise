#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 12
#define STEPS 5000
#define TEMP 1.0f

typedef struct {
    float theta;   // fase principal
    float phi;     // twist
    float r;       // escala
} Toro;

Toro T[N];

// ================= RANDOM FLOAT =================
float frand(){
    return (float)rand() / (float)RAND_MAX;
}

// ================= INIT =================
void init(){
    for(int i=0;i<N;i++){
        T[i].theta = frand() * 2*M_PI;
        T[i].phi   = frand() * 2*M_PI;
        T[i].r     = 0.5f + frand(); // escala
    }
}

// ================= DISTÂNCIA ANGULAR =================
float ang_diff(float a, float b){
    float d = fabs(a - b);
    if(d > M_PI) d = 2*M_PI - d;
    return d;
}

// ================= INTERSEÇÃO (GEOMÉTRICA SIMPLIFICADA) =================
float interaction(int i, int j){
    float dtheta = ang_diff(T[i].theta, T[j].theta);
    float dphi   = ang_diff(T[i].phi,   T[j].phi);

    // tentativa de "passar por dentro"
    float cross = exp(-dtheta*dtheta*4) * exp(-dphi*dphi*4);

    // penaliza colisão impossível
    float size_penalty = fabs(T[i].r - T[j].r);

    return cross * (1.0f + size_penalty);
}

// ================= ENERGIA =================
float energy(){
    float E = 0.0f;

    for(int i=0;i<N;i++){
        for(int j=i+1;j<N;j++){

            float align = cos(T[i].theta - T[j].theta);
            float twist = cos(T[i].phi   - T[j].phi);

            float inter = interaction(i,j);

            E += (1.0f - align);
            E += 0.5f * (1.0f - twist);
            E += 2.0f * inter; // peso topológico
        }
    }

    return E;
}

// ================= CONTAR CONFLITOS =================
int count_conflicts(){
    int c = 0;

    for(int i=0;i<N;i++){
        for(int j=i+1;j<N;j++){
            float dtheta = ang_diff(T[i].theta, T[j].theta);
            if(dtheta < 0.2) c++;
        }
    }

    return c;
}

// ================= "LINKING" APROX =================
float linking_metric(){
    float L = 0.0f;

    for(int i=0;i<N;i++){
        for(int j=i+1;j<N;j++){
            L += sin(T[i].theta - T[j].theta) * sin(T[i].phi - T[j].phi);
        }
    }

    return L;
}

// ================= MOVE =================
void move(){
    int i = rand() % N;

    T[i].theta += (frand() - 0.5f) * 0.5f;
    T[i].phi   += (frand() - 0.5f) * 0.5f;
    T[i].r     += (frand() - 0.5f) * 0.2f;

    if(T[i].theta < 0) T[i].theta += 2*M_PI;
    if(T[i].theta > 2*M_PI) T[i].theta -= 2*M_PI;

    if(T[i].phi < 0) T[i].phi += 2*M_PI;
    if(T[i].phi > 2*M_PI) T[i].phi -= 2*M_PI;

    if(T[i].r < 0.1f) T[i].r = 0.1f;
    if(T[i].r > 2.0f) T[i].r = 2.0f;
}

// ================= MAIN =================
int main(){
    srand(time(NULL));

    init();

    float E = energy();

    printf("=== VECTRA TOROIDAL SYSTEM ===\n");

    for(int t=0;t<STEPS;t++){

        Toro backup = T[rand()%N]; // backup parcial

        move();

        float E2 = energy();

        // Metropolis
        if(E2 < E || exp((E - E2)/TEMP) > frand()){
            E = E2;
        } else {
            // rollback parcial simples
            int idx = rand()%N;
            T[idx] = backup;
        }

        if(t % 500 == 0){
            printf("t=%d E=%f conflicts=%d link=%f\n",
                t, E, count_conflicts(), linking_metric());
        }
    }

    printf("\nFinal Energy: %f\n", E);
    printf("Final Conflicts: %d\n", count_conflicts());
    printf("Final Linking: %f\n", linking_metric());

    return 0;
}
