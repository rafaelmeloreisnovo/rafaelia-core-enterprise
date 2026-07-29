# rafaelia-core-enterprise

**Estado:** `ACTIVE`
**Proprietário lógico:** `research-governance`
**Repositório:** [`rafaelmeloreisnovo/rafaelia-core-enterprise`](https://github.com/rafaelmeloreisnovo/rafaelia-core-enterprise)

Coleção de implementações C de nível enterprise para o ecossistema RAFAELIA: variantes do motor Vectra, computações geodésicas (GeoLM), visualizações FFT fractais, análise de texto Voynich e executores T⁷.

## Arquivos principais

| Arquivo | Papel | Estado |
|---|---|---|
| `Incluir.md` | Documentação técnica central (90 KB) | `ACTIVE` |
| `fractal-fft-plotter.html` | Visualizador FFT fractal interativo (HTML/JS) | `ACTIVE` |
| `geolm.c` / `geolm_raf.c` | Motor geodésico GeoLM (48 KB) — cálculos de geodésica em superfícies | `ACTIVE` |
| `geolm_raf-1.c` / `geolm_raf-2.c` | Variantes refatoradas do GeoLM | `ACTIVE` |
| `vectra_42_attractors.c` | 42 atratores T⁷ do toróide 7-dimensional | `ACTIVE` |
| `vectra_t7_executor.c` | Executor do pipeline T⁷ (6.5 KB) | `ACTIVE` |
| `vectra_universal.c` | Variante universal do Vectra (5.8 KB) | `ACTIVE` |
| `vectras_bbs.c` | Servidor BBS Vectra (23 KB) | `ACTIVE` |
| `vectra_bench*.c` | Benchmarks de performance (5 variantes) | `EVIDENCE` |
| `voynich_analysis.py` | Análise Python do manuscrito Voynich (10.6 KB) | `ACTIVE` |
| `voynich_exacordex.c` / `voynich_toroidal.c` | Motor Voynich + pipeline toroidal (11.8 KB) | `ACTIVE` |
| `voynich_downloader.c` | Downloader de dados Voynich (14.4 KB) | `ACTIVE` |
| `sat_annealing.c` | Simulated annealing | `ACTIVE` |
| `ser_vivo.c` | Ser Vivo — instância executável viva | `ACTIVE` |
| `test_exacordex.c` | Testes do motor Exacordex | `EVIDENCE` |
| `vectra_*.c` (50+ variantes) | Motor Vectra: adaptive, attack, barrier, breaker, clay7, collider, entangled, extreme, fractal, geom, hypermatrix, layers, native, observer, orchestrator, portable, quantum_*, raw, singularity, structured, toroidal, torus, zero, etc. | `ACTIVE` |
| `NODE_INFO.txt` | Informações do nó de execução | `REFERENCE` |

## Famílias de código

### Motor Vectra
Implementações C das primitivas do motor RAFAELIA com 50+ variantes especializadas:
- **Benchmark**: `vectra_bench*.c`, `vectra_bench_armv7.c` — medição de throughput
- **Segurança**: `vectra_attack.c`, `vectra_barrier.c`, `vectra_breaker.c` — análise de adversários
- **Geometria**: `vectra_fractal.c`, `vectra_geom.c`, `vectra_hypermatrix.c`, `vectra_toroidal_full.c`
- **Quântico**: `vectra_quantum.c`, `vectra_quantum_flux.c`, `vectra_quantum_observer.c`, `vectra_quantum_sentinel.c`
- **Clay**: `vectra_clay7_full.c`, `vectra_clay7_v2.c` — conexão com os 7 Problemas Clay

### GeoLM — Geometria e Geodésica
Motor de cálculo geodésico em superfícies curvas, usado como base para `GEOMETRIA_SOLAR_Maia_Inca` e `Catalogo-cosmologico`.

### Análise Voynich
Pipeline de análise do manuscrito Voynich via Exacordex e estrutura toroidal T⁷.

## Build

```bash
gcc -O2 -o vectra_universal vectra_universal.c -lm
gcc -O2 -o vectra_t7 vectra_t7_executor.c -lm
gcc -O2 -o geolm geolm.c -lm
python3 voynich_analysis.py
```

## Estados de Evidência

| Gate | Estado |
|---|---|
| Todas as variantes Vectra compilam sem warnings (gcc -Wall) | `TOKEN_VAZIO` |
| Benchmark `vectra_bench.c` executado em aarch64 com resultado documentado | `TOKEN_VAZIO` |
| GeoLM validado com coordenadas de referência (geodésica WGS84) | `TOKEN_VAZIO` |
| Análise Voynich produz saída reproduzível | `TOKEN_VAZIO` |

## Referências

- [`RafPolimata`](https://github.com/rafaelmeloreisnovo/RafPolimata) — pipeline APKc e motor Vectra em alto nível
- [`ChipQuantum`](https://github.com/rafaelmeloreisnovo/ChipQuantum) — C/ASM, geometria, criptografia
- [`GEOMETRIA_SOLAR_Maia_Inca`](https://github.com/rafaelmeloreisnovo/GEOMETRIA_SOLAR_Maia_Inca) — aplicação das geodésicas GeoLM
