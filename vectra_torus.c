#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 4
#define STEPS 5000

typedef struct {
    float phase;
    float twist;
    float radius;
} Torus;

Torus T[N];

// energia de interação
float energy(){
    float E = 0;

    for(int i=0;i<N;i++){
        for(int j=i+1;j<N;j++){
            float dphi = fabs(T[i].phase - T[j].phase);
            float dtwist = fabs(T[i].twist - T[j].twist);

            // interferência
            E += cos(dphi) + 0.5f * sin(dtwist);
        }
    }

    return E;
}

// pulso toroidal
void pulse(){
    for(int i=0;i<N;i++){
        T[i].phase += 0.1f;
    }
}

// torção + tentativa de passagem
void move(){
    int i = rand()%N;

    if(rand()%2){
        T[i].twist += ((float)rand()/RAND_MAX - 0.5f);
    } else {
        T[i].phase += ((float)rand()/RAND_MAX - 0.5f);
    }
}

void init(){
    for(int i=0;i<N;i++){
        T[i].phase = (float)rand()/RAND_MAX * 2*M_PI;
        T[i].twist = (float)rand()/RAND_MAX * 2*M_PI;
        T[i].radius = 1.0f;
    }
}

int main(){
    srand(time(NULL));

    init();

    float E = energy();

    printf("=== TOROIDAL SYSTEM ===\n");

    for(int t=0;t<STEPS;t++){

        Torus backup[N];
        for(int i=0;i<N;i++) backup[i] = T[i];

        pulse();
        move();

        float E2 = energy();

        if(E2 < E || exp((E - E2)) > (float)rand()/RAND_MAX){
            E = E2;
        } else {
            for(int i=0;i<N;i++) T[i] = backup[i];
        }

        if(t%500==0){
            printf("t=%d E=%f\n",t,E);
        }
    }

    printf("\nFinal Energy: %f\n",E);

    return 0;
}
