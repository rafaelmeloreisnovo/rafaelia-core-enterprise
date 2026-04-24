#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Declarações das funções assembly
extern uint32_t exacordex_inject_massive(const void *word, size_t len, uint32_t seed);
extern void exacordex_reconstruct_phase(uint32_t index, void *dest);
extern void exacordex_fuse(uint32_t idx_a, uint32_t idx_b, uint32_t idx_dest);
extern uint32_t exacordex_search_nearest(const void *query, uint32_t *best_index);
extern void* get_torus_base(void);

// Função auxiliar para imprimir vetor de 16 bytes
void print_vector(const char *label, const uint8_t *vec) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) printf("%02x ", vec[i]);
    printf("\n");
}

int main() {
    printf("=== EXACORDEX – TESTE INTEGRADO ===\n\n");

    // 1. Injetar duas palavras
    const char *word1 = "amor";
    const char *word2 = "amor";  // mesma palavra, mas será injetada em momentos diferentes
    uint32_t idx1 = exacordex_inject_massive(word1, strlen(word1), 0x12345678);
    uint32_t idx2 = exacordex_inject_massive(word2, strlen(word2), 0x87654321);
    printf("Injeção 1 (\"%s\") -> índice 0x%06X\n", word1, idx1);
    printf("Injeção 2 (\"%s\") -> índice 0x%06X\n", word2, idx2);

    // 2. Reconstruir a partir do índice
    uint8_t reconstructed[16] = {0};
    exacordex_reconstruct_phase(idx1, reconstructed);
    print_vector("Reconstruído (índice 0)", reconstructed);

    // 3. Busca por ressonância (vizinho mais próximo)
    uint32_t best_idx;
    uint32_t dist = exacordex_search_nearest(reconstructed, &best_idx);
    printf("Busca: vetor reconstruído -> melhor índice 0x%06X (distância %u)\n", best_idx, dist);

    // 4. Fusão harmônica entre os dois índices (se diferentes)
    if (idx1 != idx2) {
        uint32_t idx_fused = 0x3FFFFF; // um slot livre (último, por exemplo)
        exacordex_fuse(idx1, idx2, idx_fused);
        uint8_t fused_vec[16];
        exacordex_reconstruct_phase(idx_fused, fused_vec);
        print_vector("Fusão de amor + amor (com jitter)", fused_vec);
    } else {
        printf("Os índices são iguais (micro‑entropia não ativada).\n");
    }

    // 5. Mostrar endereço base do Toro (apenas curiosidade)
    void *base = get_torus_base();
    printf("Toro 7D base: %p (tamanho 64 MB)\n", base);

    printf("\nTeste concluído.\n");
    return 0;
}
