#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define N 20
#define STEPS 5000

typedef struct {
    float x, y;
    float angle;
} Node;

Node state[N];

// energia = erro de encaixe geométrico
float energy(){
    float E = 0.0f;

    for(int i=0;i<N-1;i++){
        float dx = state[i].x - state[i+1].x;
        float dy = state[i].y - state[i+1].y;

        float dist = sqrt(dx*dx + dy*dy);

        // queremos distância ~1
        E += fabs(dist - 1.0f);

        // alinhamento angular
        float da = fabs(state[i].angle - state[i+1].angle);
        E += da * 0.1f;
    }

    return E;
}

// permutação multinível
void move(){
    int t = rand()%3;

    int i = rand()%N;

    if(t==0){
        // mover posição
        state[i].x += ((float)rand()/RAND_MAX - 0.5f)*0.5f;
        state[i].y += ((float)rand()/RAND_MAX - 0.5f)*0.5f;
    }
    else if(t==1){
        // rotacionar
        state[i].angle += ((float)rand()/RAND_MAX - 0.5f)*0.5f;
    }
    else{
        // swap estrutural
        int j = rand()%N;
        Node tmp = state[i];
        state[i] = state[j];
        state[j] = tmp;
    }
}

void init(){
    for(int i=0;i<N;i++){
        state[i].x = (float)rand()/RAND_MAX;
        state[i].y = (float)rand()/RAND_MAX;
        state[i].angle = (float)rand()/RAND_MAX * 2*M_PI;
    }
}

int main(){
    srand(time(NULL));

    init();

    float E = energy();

    printf("=== GEOMETRIC DYNAMICS ===\n");

    for(int t=0;t<STEPS;t++){
        Node backup = state[rand()%N];

        move();

        float E2 = energy();

        // Metropolis
        if(E2 < E || exp((E - E2)) > (float)rand()/RAND_MAX){
            E = E2;
        }

        if(t%500==0){
            printf("t=%d E=%f\n",t,E);
        }
    }

    printf("\nFinal Energy: %f\n",E);

    return 0;
}
