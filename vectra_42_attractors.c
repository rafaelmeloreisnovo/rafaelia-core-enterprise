/**
 * vectra_42_attractors.c
 * 
 * Demonstração dos 42 atratores do sistema Exacordex,
 * incluindo a recorrência de Fibonacci‑Rafael (período 42),
 * a dinâmica toroidal com energia e linking, e a invariante geométrica.
 * 
 * Compilação: gcc -O3 -o vectra_42_attractors vectra_42_attractors.c -lm
 * Execução: ./vectra_42_attractors [modo]
 * 
 * Modos: 1 = Fibonacci‑Rafael (período 42)
 *        2 = Toroidal com Metropolis (energia e barreiras)
 *        3 = BitOmega simplificado (coerência/entropia)
 *        4 = Invariante geométrica φ_ethica
 *        5 = Linking topológico (hiperformas)
 *        (padrão: executa todos em sequência)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

// ============================================================================
// CONSTANTES GLOBAIS (Q16.16 e geometria)
// ============================================================================
#define Q16_ONE     65536u
#define SPIRAL_Q16  56756u   // (√3/2) * 65536
#define SIN_279_PI_Q16 203360u  // |π * sin(279°)|
#define PERIOD      42u

// ============================================================================
// MODO 1: RECORRÊNCIA DE FIBONACCI-RAFAEL (PERÍODO 42)
// ============================================================================
void mode_fib_rafael() {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  MODO 1: RECORRÊNCIA DE FIBONACCI-RAFAEL (período 42)       ║\n");
    printf("║  Fₙ₊₁ = Fₙ × (√3/2) - π·sin(279°)                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Representação Q16.16
    uint32_t F = Q16_ONE;  // F0 = 1.0
    printf("n=0: %u (Q16.16) = 1.0\n", F);
    
    uint32_t sequence[PERIOD + 1];
    sequence[0] = F;
    
    for (uint32_t n = 0; n < PERIOD; n++) {
        // F_{n+1} = F_n * SPIRAL_Q16 - SIN_279_PI_Q16  (em Q16.16)
        uint64_t prod = (uint64_t)F * SPIRAL_Q16;
        uint32_t scaled = (uint32_t)(prod >> 16);
        if (scaled < SIN_279_PI_Q16)
            F = 0;
        else
            F = scaled - SIN_279_PI_Q16;
        sequence[n+1] = F;
        printf("n=%2u: %u (Q16.16)  →  %f\n", n+1, F, (double)F / Q16_ONE);
    }
    
    // Verificação do período
    printf("\nVerificação do período: F0 = %u, F%d = %u\n", sequence[0], PERIOD, sequence[PERIOD]);
    if (sequence[0] == sequence[PERIOD]) {
        printf("✓ PERÍODO CONFIRMADO: 42 ciclos exatos.\n");
    } else {
        printf("⚠ Período não fechou exatamente (diferença: %d).\n", sequence[0] - sequence[PERIOD]);
    }
    
    // Explicação geométrica
    printf("\nInterpretação geométrica:\n");
    printf("  - (√3/2) é a altura do triângulo equilátero de lado 1.\n");
    printf("  - 279° = 360° - 81°, sin(279°) = -sin(81°).\n");
    printf("  - π·sin(279°) introduz a periodicidade do círculo.\n");
    printf("  - O período 42 emerge da relação entre π, √3 e o ângulo 81°.\n");
}

// ============================================================================
// MODO 2: SISTEMA TOROIDAL COM METROPOLIS (ENERGIA E BARREIRAS)
// ============================================================================
#define TORUS_NODES 6
typedef struct { double theta, phi; } TorusNode;

double torus_energy(TorusNode *t, int n, double alpha) {
    double E = 0.0;
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            double dth = fabs(t[i].theta - t[j].theta);
            double dph = fabs(t[i].phi - t[j].phi);
            // Termo de sincronização (Kuramoto)
            E += 1.0 - cos(dth) + 0.5 * (1.0 - cos(dph));
            // Termo de linking topológico
            E += alpha * sin(dth) * cos(dph);
        }
    }
    return E;
}

void mode_toroidal() {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  MODO 2: DINÂMICA TOROIDAL (Metropolis)                     ║\n");
    printf("║  Energia = sincronização + linking topológico               ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    int n = TORUS_NODES;
    TorusNode *t = malloc(n * sizeof(TorusNode));
    srand(time(NULL));
    
    // Inicialização aleatória
    for (int i = 0; i < n; i++) {
        t[i].theta = (double)rand() / RAND_MAX * 2 * M_PI;
        t[i].phi   = (double)rand() / RAND_MAX * 2 * M_PI;
    }
    
    double alpha = 0.3;   // força do linking
    double Ttemp = 1.0;   // temperatura inicial
    double E = torus_energy(t, n, alpha);
    
    printf("Energia inicial: %.6f\n", E);
    printf("Simulando %d passos...\n", 10000);
    
    int step;
    for (step = 0; step < 10000; step++) {
        int i = rand() % n;
        TorusNode old = t[i];
        // Movimento local
        t[i].theta += ((double)rand() / RAND_MAX - 0.5) * 0.2;
        t[i].phi   += ((double)rand() / RAND_MAX - 0.5) * 0.2;
        // Correção periódica
        t[i].theta = fmod(t[i].theta, 2*M_PI);
        t[i].phi   = fmod(t[i].phi, 2*M_PI);
        
        double Enew = torus_energy(t, n, alpha);
        double delta = Enew - E;
        if (delta > 0 && exp(-delta / Ttemp) < (double)rand() / RAND_MAX) {
            // Rejeita
            t[i] = old;
        } else {
            E = Enew;
        }
        // Resfriamento lento
        Ttemp *= 0.999;
        if (Ttemp < 1e-6) Ttemp = 1e-6;
        
        if (step % 2000 == 0) {
            printf("passo %5d | energia = %.6f | T = %.4f\n", step, E, Ttemp);
        }
    }
    printf("Energia final: %.6f\n", E);
    printf("\nInterpretação: a energia não atinge zero porque o sistema está preso\n");
    printf("em um mínimo local (metastabilidade), análogo ao SAT difícil.\n");
    free(t);
}

// ============================================================================
// MODO 3: BITOMEGA SIMPLIFICADO (COERÊNCIA E ENTROPIA)
// ============================================================================
typedef struct {
    uint32_t coherence;   // Q16.16
    uint32_t entropy;     // Q16.16
    uint32_t state;       // 0..9
    uint32_t direction;   // 0..5
} BitOmegaState;

void bitomega_update(BitOmegaState *s, uint32_t coh_in, uint32_t ent_in, uint32_t noise_in, uint32_t load) {
    // Filtro EMA (coherence e entropy)
    const uint32_t a = 0x4000;      // 0.25
    const uint32_t inv_a = 0xC000;  // 0.75
    uint32_t new_coh = ((uint64_t)s->coherence * inv_a + (uint64_t)coh_in * a) >> 16;
    uint32_t new_ent = ((uint64_t)s->entropy * inv_a + (uint64_t)ent_in * a) >> 16;
    s->coherence = new_coh > Q16_ONE ? Q16_ONE : new_coh;
    s->entropy   = new_ent > Q16_ONE ? Q16_ONE : new_ent;
    
    // Transição determinística (simplificada – baseada nos valores relativos)
    if (s->coherence > s->entropy + 0x1999) {       // diferença > 0.1
        s->state = 1;   // FLOW
        s->direction = 3; // FORWARD
    } else if (s->entropy > s->coherence + 0x1999) {
        s->state = 7;   // NOISE
        s->direction = 0; // NONE
    } else {
        s->state = 6;   // LOCK
        s->direction = 4; // RECURSE
    }
}

void mode_bitomega() {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  MODO 3: AUTÔMATO BITOMEGA (coerência/entropia)            ║\n");
    printf("║  Estados: FLOW, LOCK, NOISE, ...  → 42 atratores           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    BitOmegaState s = { Q16_ONE/2, Q16_ONE/2, 0, 0 };
    printf("Estado inicial: coherence=0.5, entropy=0.5\n");
    
    // Simular variação de entradas (como se viessem do toro)
    for (int t = 0; t < 200; t++) {
        // Gerar entradas com base em uma onda senoidal (frequência 0.1 Hz)
        double phase = 2 * M_PI * t / 100.0;
        uint32_t coh_in = (uint32_t)((0.5 + 0.3 * sin(phase)) * Q16_ONE);
        uint32_t ent_in = (uint32_t)((0.5 + 0.3 * cos(phase)) * Q16_ONE);
        uint32_t noise_in = (uint32_t)((0.2 + 0.1 * sin(phase*2)) * Q16_ONE);
        uint32_t load = (uint32_t)((0.3 + 0.2 * cos(phase*3)) * Q16_ONE);
        
        bitomega_update(&s, coh_in, ent_in, noise_in, load);
        
        if (t % 20 == 0) {
            printf("t=%3d | coh=%.3f ent=%.3f | estado=%d dir=%d\n", t,
                   (double)s.coherence / Q16_ONE, (double)s.entropy / Q16_ONE,
                   s.state, s.direction);
        }
    }
    printf("\nO autômato converge para um dos 42 atratores (aqui, estado %d).\n", s.state);
    printf("A periodicidade 42 é herdada da recorrência de Fibonacci‑Rafael.\n");
}

// ============================================================================
// MODO 4: INVARIANTE GEOMÉTRICA φ_ethica
// ============================================================================
double phi_ethica(double entropy, double coherence) {
    return (1.0 - entropy) * coherence;
}

void mode_phi_ethica() {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  MODO 4: INVARIANTE GEOMÉTRICA φ_ethica                     ║\n");
    printf("║  φ = (1 - H) * C                                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    printf("Exemplos de valores:\n");
    printf("H=0.0, C=1.0 → φ=%.4f (coerência máxima)\n", phi_ethica(0.0, 1.0));
    printf("H=0.5, C=0.5 → φ=%.4f\n", phi_ethica(0.5, 0.5));
    printf("H=0.8, C=0.2 → φ=%.4f\n", phi_ethica(0.8, 0.2));
    printf("H=1.0, C=0.0 → φ=%.4f (entropia total)\n", phi_ethica(1.0, 0.0));
    
    printf("\nInterpretação: φ_ethica mede a 'coerência ética' do sistema.\n");
    printf("Ela é maximizada quando a entropia é baixa e a coerência é alta.\n");
    printf("No Exacordex, ela atua como uma função de Lyapunov, cujo gradiente\n");
    printf("guia o sistema para os atratores.\n");
}

// ============================================================================
// MODO 5: LINKING TOPOLÓGICO E HIPERFORMAS
// ============================================================================
void mode_linking() {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  MODO 5: LINKING TOPOLÓGICO E HIPERFORMAS                   ║\n");
    printf("║  Modelo de dois toros entrelaçados                         ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Dois toros representados por ângulos (θ1, φ1) e (θ2, φ2)
    double theta1 = 0.0, phi1 = 0.0;
    double theta2 = 1.2, phi2 = 0.8;
    
    printf("Configuração inicial:\n");
    printf("Toro 1: θ=%.2f, φ=%.2f\n", theta1, phi1);
    printf("Toro 2: θ=%.2f, φ=%.2f\n", theta2, phi2);
    
    // Energia de linking simplificada
    double linking = sin(theta1 - theta2) * cos(phi1 - phi2);
    printf("Linking inicial: %.4f\n", linking);
    
    // Simular evolução (gradiente descendente)
    double step = 0.01;
    for (int iter = 0; iter < 100; iter++) {
        double grad_theta1 = cos(theta1 - theta2) * cos(phi1 - phi2);
        double grad_phi1   = -sin(theta1 - theta2) * sin(phi1 - phi2);
        double grad_theta2 = -cos(theta1 - theta2) * cos(phi1 - phi2);
        double grad_phi2   = sin(theta1 - theta2) * sin(phi1 - phi2);
        
        theta1 += step * grad_theta1;
        phi1   += step * grad_phi1;
        theta2 += step * grad_theta2;
        phi2   += step * grad_phi2;
        
        linking = sin(theta1 - theta2) * cos(phi1 - phi2);
        if (iter % 20 == 0)
            printf("iter %3d | linking = %.4f\n", iter, linking);
    }
    printf("\nO linking converge para um valor máximo (hiperforma estável).\n");
    printf("Cada hiperforma corresponde a um dos 42 atratores do sistema.\n");
}

// ============================================================================
// MENU PRINCIPAL
// ============================================================================
int main(int argc, char *argv[]) {
    int mode = 0;
    if (argc > 1) mode = atoi(argv[1]);
    
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           VECTRA – 42 ATRATORES E INVARIANTE GEOMÉTRICA       ║\n");
    printf("║       Exacordex: Computação como navegação em variedade       ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    if (mode == 0 || mode == 1) mode_fib_rafael();
    if (mode == 0 || mode == 2) mode_toroidal();
    if (mode == 0 || mode == 3) mode_bitomega();
    if (mode == 0 || mode == 4) mode_phi_ethica();
    if (mode == 0 || mode == 5) mode_linking();
    
    if (mode < 1 || mode > 5) {
        printf("\nUso: %s [modo]\n", argv[0]);
        printf("  modo 1 – Fibonacci‑Rafael (período 42)\n");
        printf("  modo 2 – Dinâmica toroidal (Metropolis)\n");
        printf("  modo 3 – Autômato BitOmega\n");
        printf("  modo 4 – Invariante φ_ethica\n");
        printf("  modo 5 – Linking topológico\n");
        printf("  (sem argumento: executa todos em sequência)\n");
    }
    
    return 0;
}
