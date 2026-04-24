#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#  include <termios.h>
#  include <sys/ioctl.h>
#  define HAVE_TERMIOS 1
#else
#  define HAVE_TERMIOS 0
#endif

#if defined(__arm__) || defined(__aarch64__)
#  define RMR_ARM 1
#else
#  define RMR_ARM 0
#endif

/* ANSI */
#define ESC   "\033["
#define CLR   ESC "2J" ESC "H"
#define BOLD  ESC "1m"
#define DIM   ESC "2m"
#define RESET ESC "0m"
#define CYN   ESC "36m"
#define YEL   ESC "33m"
#define GRN   ESC "32m"
#define RED   ESC "31m"
#define BLU   ESC "34m"
#define MAG   ESC "35m"
#define WHT   ESC "37m"

/* Linhas decorativas em ASCII puro (sem Unicode) */
#define HL  "========================================================="
#define HL2 "---------------------------------------------------------"

#define BBS_NAME "VECTRAS BBS"
#define BBS_VER  "v1.0 ARM32"
#define MAX_LINE  256
#define BENCH_WARM  5
#define BENCH_RUNS 20
#define BENCH_PARAM 64

/* ==================== TERMINAL ================================= */
static int term_cols = 78;

static void term_detect(void) {
#if HAVE_TERMIOS
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 20)
        term_cols = w.ws_col;
#endif
}

static void term_clear(void) { printf(CLR); fflush(stdout); }

static void term_line(void) {
    printf(DIM);
    for (int i = 0; i < term_cols && i < 78; i++) putchar('=');
    printf(RESET "\n");
}

static void term_dline(void) {
    printf(DIM);
    for (int i = 0; i < term_cols && i < 78; i++) putchar('-');
    printf(RESET "\n");
}

static void term_pause(void) {
    printf(DIM "\n  [ENTER para continuar]" RESET);
    fflush(stdout);
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

/* ==================== TEMPO ==================================== */
static uint64_t rmr_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void rmr_sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ==================== NUCLEO MATEMATICO ======================== */
#define PHI32  0x9E3779B9u
#define FNV1A  0x100000001B3ULL
#define FNV_B  0xCBF29CE484222325ULL
#define ALPHA_Q8 64

typedef struct { uint32_t u,v,psi,chi,rho,delta,sigma; } State7D;
typedef struct { uint32_t c_q8, h_q8, stage; uint64_t hash; } Kernel;

static uint32_t rmr_update(uint32_t s, uint32_t x) {
    return (((256u - ALPHA_Q8) * s) >> 8) + ((ALPHA_Q8 * x) >> 8);
}

static uint64_t rmr_fnv(const uint8_t *d, size_t n) {
    uint64_t h = FNV_B;
    for (size_t i = 0; i < n; i++) h = (h ^ d[i]) * FNV1A;
    return h;
}

static uint32_t rmr_entropy(const uint8_t *d, size_t n) {
    uint32_t f[256] = {0};
    for (size_t i = 0; i < n; i++) f[d[i]]++;
    uint32_t u = 0, t = 0;
    for (int i = 0; i < 256; i++) if (f[i]) u++;
    for (size_t i = 1; i < n; i++) if (d[i] != d[i-1]) t++;
    uint32_t e = (u * 6000u) / 256u;
    if (n > 1) e += (t * 2000u) / (uint32_t)(n - 1);
    return e > 8000u ? 8000u : e;
}

static State7D rmr_tmap(uint32_t seed, uint64_t hash, uint32_t ent, uint32_t st) {
    State7D s;
    s.u     = ((seed ^ (uint32_t)(hash >> 32)) * PHI32) & 0xFFFF;
    s.v     = ((seed ^ (uint32_t)(hash))       * PHI32) & 0xFFFF;
    s.psi   = ((ent  * PHI32) >> 16) & 0xFFFF;
    s.chi   = (((st+1u) * PHI32) >> 16) & 0xFFFF;
    s.rho   = ((s.u ^ s.v)     * PHI32) & 0xFFFF;
    s.delta = ((s.psi ^ s.chi) * PHI32) & 0xFFFF;
    s.sigma = ((s.rho ^ s.delta)* PHI32)& 0xFFFF;
    return s;
}

static void kernel_init(Kernel *k, uint32_t seed) {
    k->c_q8 = 128; k->h_q8 = 128;
    k->hash = (uint64_t)seed * PHI32; k->stage = 0;
}

static State7D kernel_ingest(Kernel *k, const char *txt) {
    size_t n = strlen(txt);
    uint64_t h = rmr_fnv((const uint8_t *)txt, n);
    uint32_t e = rmr_entropy((const uint8_t *)txt, n);
    k->hash  = (k->hash ^ h) * FNV1A;
    k->h_q8  = rmr_update(k->h_q8, (e * 255u) / 8000u);
    k->c_q8  = rmr_update(k->c_q8, 255u - k->h_q8);
    k->stage++;
    return rmr_tmap(0x52414641u, k->hash, k->h_q8, k->stage);
}

/* ==================== ASCII ART ================================ */
static const char *LOGO[] = {
"  __   _____ ___ _____ ___    _   ___",
"  \\ \\ / / __/ __|_   _| _ \\  /_\\ / __|",
"   \\ V /| _| (__  | | |   / / _ \\\\__ \\",
"    \\_/ |___\\___| |_| |_|_\\/_/ \\_\\___/",
"  BBS  --  ARM32/ARM64/x86  --  v1.0",
NULL
};

static const char *BBS_ART[] = {
"  +-------------------------------------------+",
"  |  ____  ____  ____                         |",
"  | | __ )| __ )/ ___|                        |",
"  | |  _ \\|  _ \\\\___ \\   VECTRAS BBS          |",
"  | | |_) | |_) |___) |  Telnet/ASCII         |",
"  | |____/|____/|____/   ARM32 Compatible     |",
"  +-------------------------------------------+",
NULL
};

static const char *BROWSER_ART[] = {
"  +--------------------------------------------+",
"  |  _____ ____  ____   _____        ______    |",
"  | |_   _| __ )|  _ \\ / _ \\ \\      / / ___|   |",
"  |   | | |  _ \\| |_) | | | \\ \\ /\\ / /\\___ \\   |",
"  |   | | | |_) |  _ <| |_| |\\ V  V /  ___) |  |",
"  |   |_| |____/|_| \\_\\\\___/  \\_/\\_/  |____/   |",
"  +--------------------------------------------+",
NULL
};

static void print_logo(const char **lines, const char *color) {
    for (int i = 0; lines[i]; i++)
        printf("%s%s" RESET "\n", color, lines[i]);
}

/* ==================== BBS ====================================== */
typedef struct {
    char author[32], subject[64], body[256];
    uint32_t id;
} BBSPost;

#define MAX_POSTS 16
static BBSPost posts[MAX_POSTS];
static int post_count = 0;

static void bbs_seed(void) {
    const char *au[] = {"SYSOP","RAFAEL","VECTRAS","ANON","RMR"};
    const char *su[] = {
        "Bem-vindo ao VECTRAS BBS",
        "Sistema RMR ARM32",
        "Topologia Toroidal T^7",
        "Benchmark resultados",
        "ASCII Art compilado",
        "Termux + BBS setup",
        "Teoria dos atratores",
        "Hash FNV-1a 64bit"
    };
    const char *bo[] = {
        "Este BBS roda em C puro, sem dependencias externas.",
        "Kernel RMR: estado 7D em toro, alpha=0.25, BIBO estavel.",
        "s=(u,v,psi,chi,rho,delta,sigma) in [0,65535]^7",
        "Mediana de 20 runs, pre-aquecimento 5 ciclos.",
        "Compilavel em ARM32, ARM64, x86_64 com gcc/clang.",
        "pkg install clang && clang -O2 -o bbs bbs.c && ./bbs",
        "Atratores: lim s(t) converge para conjunto finito A.",
        "h = (h XOR byte) * 0x100000001B3 -- FNV prime"
    };
    for (int i = 0; i < 8; i++) {
        snprintf(posts[i].author,  32, "%s", au[i%5]);
        snprintf(posts[i].subject, 64, "%s", su[i]);
        snprintf(posts[i].body,   256, "%s", bo[i]);
        posts[i].id = (uint32_t)(i + 1);
    }
    post_count = 8;
}

static void bbs_header(void) {
    term_clear();
    term_line();
    print_logo(BBS_ART, CYN BOLD);
    printf(YEL "  %s  |  %s\n" RESET, BBS_NAME, BBS_VER);
    term_line();
}

static void bbs_list(void) {
    bbs_header();
    printf(BOLD WHT "\n  #   AUTOR          ASSUNTO\n" RESET);
    term_dline();
    for (int i = 0; i < post_count; i++)
        printf("  " YEL "%2d" RESET "  %-14s %s\n",
               posts[i].id, posts[i].author, posts[i].subject);
    term_dline();
    printf(DIM "\n  [1-%d] ler  [N] novo  [Q] sair\n" RESET, post_count);
}

static void bbs_read(int idx) {
    if (idx < 0 || idx >= post_count) return;
    BBSPost *p = &posts[idx];
    term_clear();
    term_dline();
    printf(BOLD CYN "  POST #%u\n" RESET, p->id);
    printf(YEL "  De:      " WHT "%s\n" RESET, p->author);
    printf(YEL "  Assunto: " WHT "%s\n" RESET, p->subject);
    term_dline();
    printf("\n  %s\n\n", p->body);
    Kernel k; kernel_init(&k, p->id * 0x52414641u);
    State7D s = kernel_ingest(&k, p->body);
    printf(DIM "  [Toro7D] u=%u v=%u psi=%u chi=%u rho=%u delta=%u sigma=%u\n" RESET,
           s.u, s.v, s.psi, s.chi, s.rho, s.delta, s.sigma);
    printf(DIM "  [Kernel] C=%u/255 H=%u/255 hash=0x%016llX\n" RESET,
           k.c_q8, k.h_q8, (unsigned long long)k.hash);
    term_dline();
}

static void bbs_new(void) {
    if (post_count >= MAX_POSTS) { printf(RED "  Board cheio.\n" RESET); return; }
    BBSPost *p = &posts[post_count];
    printf(YEL "  Autor: " RESET); fflush(stdout);
    if (!fgets(p->author,  sizeof(p->author),  stdin)) return;
    p->author[strcspn(p->author, "\n")] = 0;
    printf(YEL "  Assunto: " RESET); fflush(stdout);
    if (!fgets(p->subject, sizeof(p->subject), stdin)) return;
    p->subject[strcspn(p->subject, "\n")] = 0;
    printf(YEL "  Mensagem: " RESET); fflush(stdout);
    if (!fgets(p->body,    sizeof(p->body),    stdin)) return;
    p->body[strcspn(p->body, "\n")] = 0;
    p->id = (uint32_t)(++post_count);
    printf(GRN "  Post #%u publicado.\n" RESET, p->id);
}

static void bbs_run(int autom) {
    bbs_seed();
    if (autom) {
        bbs_list(); rmr_sleep_ms(600);
        for (int i = 0; i < post_count; i++) { bbs_read(i); rmr_sleep_ms(300); }
        return;
    }
    char cmd[MAX_LINE];
    for (;;) {
        bbs_list();
        printf(GRN "\n  > " RESET); fflush(stdout);
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\n")] = 0;
        if (cmd[0]=='Q'||cmd[0]=='q') break;
        if (cmd[0]=='N'||cmd[0]=='n') { bbs_new(); term_pause(); continue; }
        int n = atoi(cmd);
        if (n >= 1 && n <= post_count) { bbs_read(n-1); term_pause(); }
    }
}

/* ==================== TBROWSER ================================= */
typedef struct { const char *url, *title, *content[14]; } Page;

static const Page pages[] = {
  { "rmr://home", "VECTRAS HOME", {
    "  +-----------------------------------------------+",
    "  |  VECTRAS BBS -- Sistema de Computacao Hibrida |",
    "  |  ARM32 * ARM64 * x86_64 * Termux              |",
    "  +-----------------------------------------------+",
    "",
    "  Navegacao:",
    "    [1] rmr://theory  -- Teoria do sistema",
    "    [2] rmr://math    -- Formulas matematicas",
    "    [3] rmr://bench   -- Resultados benchmark",
    "    [4] rmr://about   -- Sobre o projeto",
    NULL
  }},
  { "rmr://theory", "TEORIA RMR", {
    "  Sistema Dinamico Hibrido Permutacional",
    "  S = (X, Sigma, Pi, F)",
    "    X  -- espaco continuo de estados",
    "    Sg -- espaco simbolico",
    "    Pi -- grupo de permutacoes",
    "    F  -- funcao de evolucao",
    "",
    "  Hipoteses falsificaveis:",
    "    H1: permutacao reduz tempo de convergencia",
    "    H2: multiescala melhora robustez a ruido",
    "    H3: selecao por energia reduz entropia",
    NULL
  }},
  { "rmr://math", "MATEMATICA RMR", {
    "  [1] Atualizacao recursiva:",
    "      s_{t+1} = (1-a)*s_t + a*x_t,  a=0.25",
    "",
    "  [2] Estado toroidal T^7:",
    "      s = (u,v,psi,chi,rho,delta,sigma)",
    "",
    "  [3] Entropia milli:",
    "      H ~ unique*6000/256 + trans*2000/(N-1)",
    "",
    "  [4] Hash FNV-1a 64-bit:",
    "      h = (h XOR byte) * 0x100000001B3",
    NULL
  }},
  { "rmr://bench", "BENCHMARK", {
    "  Metricas de benchmark:",
    "    * Pre-aquecimento: 5 ciclos descartados",
    "    * Runs: 20 iteracoes",
    "    * Agregacao: MEDIANA (robusta a outliers)",
    "",
    "  Operacoes testadas:",
    "    * kernel_ingest (hash + entropia + toroidal)",
    "    * rmr_fnv64     (hash puro 512B)",
    "    * rmr_entropy   (entropia de bloco 256B)",
    NULL
  }},
  { "rmr://about", "SOBRE", {
    "  VECTRAS BBS -- C puro, zero dependencias externas",
    "  Arquitetura: ARM32 / ARM64 / x86_64",
    "  Runtime: Termux / Linux / Android",
    "",
    "  Compilar:",
    "    clang -O2 -o vectras_bbs vectras_bbs.c",
    "    ./vectras_bbs",
    "",
    "  Modos: bbs | bench | browse | demo | --help",
    NULL
  }}
};
#define N_PAGES (int)(sizeof(pages)/sizeof(pages[0]))

static void browser_render(const Page *p) {
    term_clear(); term_line();
    print_logo(BROWSER_ART, BLU BOLD);
    printf(CYN "  URL: " WHT "%s" CYN "  |  " WHT "%s\n" RESET, p->url, p->title);
    term_dline();
    for (int i = 0; p->content[i]; i++) printf(WHT "%s\n" RESET, p->content[i]);
    term_dline();
    printf(DIM "\n  [1-%d] pagina  [H] home  [Q] sair\n" RESET, N_PAGES);
}

static void browser_run(int autom) {
    int cur = 0;
    if (autom) {
        for (int i = 0; i < N_PAGES; i++) { browser_render(&pages[i]); rmr_sleep_ms(500); }
        return;
    }
    char cmd[MAX_LINE];
    for (;;) {
        browser_render(&pages[cur]);
        printf(GRN "\n  tbrowser> " RESET); fflush(stdout);
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\n")] = 0;
        if (cmd[0]=='Q'||cmd[0]=='q') break;
        if (cmd[0]=='H'||cmd[0]=='h') { cur=0; continue; }
        int n = atoi(cmd);
        if (n>=1 && n<=N_PAGES) cur = n-1;
    }
}

/* ==================== BENCHMARK ================================ */
typedef struct {
    const char *name;
    uint64_t t[BENCH_RUNS];
    uint64_t median, tmin, tmax;
    double   ops;
} BR;

static int cmp64(const void *a, const void *b) {
    uint64_t x=*(const uint64_t*)a, y=*(const uint64_t*)b;
    return (x>y)-(x<y);
}

static uint64_t bmedian(uint64_t *arr, int n) {
    uint64_t tmp[BENCH_RUNS];
    memcpy(tmp, arr, (size_t)n * sizeof(uint64_t));
    qsort(tmp, (size_t)n, sizeof(uint64_t), cmp64);
    return (n%2) ? tmp[n/2] : (tmp[n/2-1]+tmp[n/2])/2;
}

static uint64_t wk_ingest(void) {
    static char bufs[BENCH_PARAM][32];
    for (int i=0;i<BENCH_PARAM;i++) snprintf(bufs[i],32,"RMR_%d_PSI_%d",i,i*7);
    Kernel k; kernel_init(&k, 0x52414641u);
    volatile uint64_t sink=0;
    uint64_t t0=rmr_now_ns();
    for (int i=0;i<BENCH_PARAM;i++) { State7D s=kernel_ingest(&k,bufs[i]); sink^=s.u^s.sigma; }
    uint64_t t1=rmr_now_ns(); (void)sink; return t1-t0;
}

static uint64_t wk_fnv(void) {
    static uint8_t buf[512];
    for (int i=0;i<512;i++) buf[i]=(uint8_t)(i*137+42);
    volatile uint64_t sink=0;
    uint64_t t0=rmr_now_ns();
    for (int i=0;i<BENCH_PARAM;i++) { sink^=rmr_fnv(buf,512); buf[0]^=(uint8_t)i; }
    uint64_t t1=rmr_now_ns(); (void)sink; return t1-t0;
}

static uint64_t wk_ent(void) {
    static uint8_t buf[256];
    for (int i=0;i<256;i++) buf[i]=(uint8_t)i;
    volatile uint32_t sink=0;
    uint64_t t0=rmr_now_ns();
    for (int i=0;i<BENCH_PARAM;i++) { sink^=rmr_entropy(buf,256); buf[i%256]^=(uint8_t)(i+1); }
    uint64_t t1=rmr_now_ns(); (void)sink; return t1-t0;
}

typedef uint64_t(*bfn)(void);

static void bench_one(BR *r, bfn fn) {
    for (int i=0;i<BENCH_WARM;i++) fn();
    for (int i=0;i<BENCH_RUNS;i++) r->t[i]=fn();
    r->median=bmedian(r->t,BENCH_RUNS);
    r->tmin=r->tmax=r->t[0];
    for (int i=1;i<BENCH_RUNS;i++) {
        if(r->t[i]<r->tmin) r->tmin=r->t[i];
        if(r->t[i]>r->tmax) r->tmax=r->t[i];
    }
    r->ops=(r->median>0)?(double)BENCH_PARAM*1e9/(double)r->median:0.0;
}

static void bench_bar(double frac, int w) {
    int f=(int)(frac*w); if(f<0)f=0; if(f>w)f=w;
    printf(GRN "[" RESET);
    for (int i=0;i<w;i++) printf("%s%c" RESET, i<f?GRN:DIM, i<f?'#':'.');
    printf(GRN "]" RESET);
}

static void bench_hist(const BR *r) {
    uint64_t lo=r->tmin, hi=r->tmax;
    if (hi==lo) hi=lo+1;
    int bk[10]={0};
    for (int i=0;i<BENCH_RUNS;i++) {
        int b=(int)((r->t[i]-lo)*10/(hi-lo+1));
        if(b<0)b=0; if(b>9)b=9; bk[b]++;
    }
    int bmax=1; for(int i=0;i<10;i++) if(bk[i]>bmax) bmax=bk[i];
    printf(DIM "  Hist [%lluns .. %lluns]:\n" RESET,
           (unsigned long long)lo,(unsigned long long)hi);
    for (int i=0;i<10;i++) {
        uint64_t blo=lo+(uint64_t)i*(hi-lo)/10;
        printf("  %6lluns |", (unsigned long long)blo);
        int w=bk[i]*30/bmax;
        const char *c=(i<3)?GRN:(i<7)?YEL:RED;
        printf("%s",c);
        for(int j=0;j<w;j++) putchar('#');
        printf(RESET " %d\n",bk[i]);
    }
}

static void bench_full(int verbose) {
    term_clear(); term_line();
    printf(BOLD YEL "  BENCHMARK RMR  warm=%d runs=%d param=%d\n" RESET,
           BENCH_WARM,BENCH_RUNS,BENCH_PARAM);
    printf(DIM "  Arch: " RESET);
#if RMR_ARM && defined(__arm__)
    printf(GRN "ARM32\n" RESET);
#elif RMR_ARM
    printf(GRN "ARM64\n" RESET);
#else
    printf(CYN "x86_64\n" RESET);
#endif
    term_dline();

    BR res[3];
    bfn fns[3]={wk_ingest,wk_fnv,wk_ent};
    const char *nm[3]={"kernel_ingest","fnv64_512B","entropy_256B"};
    for(int i=0;i<3;i++){
        res[i].name=nm[i];
        printf(DIM "  running %-20s...\r" RESET,nm[i]); fflush(stdout);
        bench_one(&res[i],fns[i]);
    }
    printf("                                    \r");

    double mops=0;
    for(int i=0;i<3;i++) if(res[i].ops>mops) mops=res[i].ops;

    printf(BOLD "\n  RESULTADOS (mediana de %d runs):\n\n" RESET,BENCH_RUNS);
    for(int i=0;i<3;i++){
        printf("  " BOLD YEL "%-20s" RESET, res[i].name);
        printf(CYN " med:" WHT " %6lluns" RESET,(unsigned long long)res[i].median);
        printf(GRN " min:" WHT "%6lluns" RESET,(unsigned long long)res[i].tmin);
        printf(RED " max:" WHT "%6lluns" RESET,(unsigned long long)res[i].tmax);
        printf(MAG " ops/s:" WHT " %8.0f " RESET,res[i].ops);
        bench_bar(mops>0?res[i].ops/mops:0.0, 16);
        printf("\n");
    }

    if(verbose) {
        printf("\n");
        for(int i=0;i<3;i++){
            printf(BOLD CYN "\n  %s:\n" RESET,res[i].name);
            bench_hist(&res[i]);
        }
    }
    term_dline();
    int best=0;
    for(int i=1;i<3;i++) if(res[i].ops>res[best].ops) best=i;
    printf(BOLD GRN "\n  Melhor: " YEL "%s " WHT "%.0f ops/s\n" RESET,
           res[best].name,res[best].ops);
    term_dline();
}

/* ==================== CLI ====================================== */
static void cli_help(void) {
    term_clear(); term_line();
    print_logo(LOGO, CYN BOLD);
    term_dline();
    printf(BOLD WHT "\n  COMANDOS:\n\n" RESET);
    const char *c[][2]={
        {"bbs",       "Board de mensagens ASCII"},
        {"browse",    "tbrowser (navegador ASCII)"},
        {"bench",     "Benchmark com mediana"},
        {"bench -v",  "Benchmark + histogramas"},
        {"demo",      "Demo automatico completo"},
        {"state",     "Estado interno do kernel RMR"},
        {"calc",      "Hash/entropia de texto"},
        {"clear",     "Limpar tela"},
        {"help",      "Esta ajuda"},
        {"quit",      "Sair"},
        {NULL,NULL}
    };
    for(int i=0;c[i][0];i++)
        printf("  " YEL "%-12s" RESET " -- %s\n",c[i][0],c[i][1]);
    printf("\n");
    term_dline();
    printf(DIM "\n  Compilar: " CYN "clang -O2 -o vectras_bbs vectras_bbs.c\n" RESET);
    printf(DIM "  ARM32:    " CYN "arm-linux-gnueabihf-gcc -O2 ...\n" RESET);
}

static void cli_state(Kernel *k) {
    printf(BOLD CYN "\n  Estado Kernel RMR\n" RESET);
    term_dline();
    printf("  " YEL "Coerencia (C): " WHT "%3u/255  " RESET, k->c_q8);
    bench_bar((double)k->c_q8/255.0, 20); printf("\n");
    printf("  " YEL "Entropia  (H): " WHT "%3u/255  " RESET, k->h_q8);
    bench_bar((double)k->h_q8/255.0, 20); printf("\n");
    printf("  " YEL "Hash:          " WHT "0x%016llX\n" RESET, (unsigned long long)k->hash);
    printf("  " YEL "Estagio:       " WHT "%u\n" RESET, k->stage);
    term_dline();
}

static void cli_calc(Kernel *k) {
    char txt[MAX_LINE];
    printf(YEL "  Texto: " RESET); fflush(stdout);
    if (!fgets(txt, sizeof(txt), stdin)) return;
    txt[strcspn(txt, "\n")] = 0;
    if (!txt[0]) return;
    State7D s = kernel_ingest(k, txt);
    size_t n = strlen(txt);
    printf(BOLD "\n  Resultado:\n" RESET);
    printf("  " YEL "FNV-1a 64: " WHT "0x%016llX\n" RESET,
           (unsigned long long)rmr_fnv((const uint8_t*)txt, n));
    printf("  " YEL "Entropia:  " WHT "%u/8000\n" RESET,
           rmr_entropy((const uint8_t*)txt, n));
    printf("  " YEL "Toro7D:    " WHT
           "u=%u v=%u psi=%u chi=%u rho=%u delta=%u sigma=%u\n" RESET,
           s.u, s.v, s.psi, s.chi, s.rho, s.delta, s.sigma);
    printf("  " YEL "Kernel:    " WHT "C=%u H=%u stage=%u\n" RESET,
           k->c_q8, k->h_q8, k->stage);
}

static void demo_run(void) {
    printf(BOLD YEL "\n  [DEMO] BBS...\n" RESET); rmr_sleep_ms(200);
    bbs_run(1);
    printf(BOLD YEL "\n  [DEMO] tbrowser...\n" RESET); rmr_sleep_ms(200);
    browser_run(1);
    printf(BOLD YEL "\n  [DEMO] benchmark...\n" RESET); rmr_sleep_ms(200);
    bench_full(0);
    term_pause();
}

static void cli_main(void) {
    Kernel k; kernel_init(&k, (uint32_t)time(NULL));
    cli_help(); term_pause();
    char line[MAX_LINE];
    for(;;) {
        printf(BOLD CYN "\n  vectras" RESET GRN "> " RESET);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line,"\n")]=0;
        if(!strcmp(line,"quit")||!strcmp(line,"exit")||!strcmp(line,"q")) break;
        else if(!strcmp(line,"bbs"))      bbs_run(0);
        else if(!strcmp(line,"browse"))   browser_run(0);
        else if(!strcmp(line,"bench"))    bench_full(0);
        else if(!strcmp(line,"bench -v")) bench_full(1);
        else if(!strcmp(line,"demo"))     demo_run();
        else if(!strcmp(line,"state"))    cli_state(&k);
        else if(!strcmp(line,"calc"))     cli_calc(&k);
        else if(!strcmp(line,"clear"))    term_clear();
        else if(!strcmp(line,"help")||!strcmp(line,"?")) cli_help();
        else if(line[0]) {
            kernel_ingest(&k, line);
            printf(DIM "  [ingerido: %zu bytes  C=%u H=%u]\n" RESET,
                   strlen(line), k.c_q8, k.h_q8);
        }
    }
    printf(GRN "\n  Ate logo.\n\n" RESET);
}

/* ==================== MAIN ===================================== */
int main(int argc, char **argv) {
    term_detect();
    bbs_seed();
    if (argc < 2) { cli_main(); return 0; }
    const char *cmd = argv[1];
    if(!strcmp(cmd,"--help")||!strcmp(cmd,"-h")) { cli_help(); return 0; }
    if(!strcmp(cmd,"bbs"))    { bbs_run(0); return 0; }
    if(!strcmp(cmd,"browse")) { browser_run(0); return 0; }
    if(!strcmp(cmd,"bench"))  {
        int v=(argc>=3&&!strcmp(argv[2],"-v"));
        bench_full(v); return 0;
    }
    if(!strcmp(cmd,"demo"))   { demo_run(); return 0; }
    if(!strcmp(cmd,"calc"))   {
        Kernel k; kernel_init(&k, 0x52414641u);
        const char *t=(argc>=3)?argv[2]:"VECTRAS RMR ARM32";
        State7D s=kernel_ingest(&k, t);
        printf("text:    %s\n",t);
        printf("fnv64:   0x%016llX\n",
               (unsigned long long)rmr_fnv((const uint8_t*)t,strlen(t)));
        printf("entropy: %u\n",rmr_entropy((const uint8_t*)t,strlen(t)));
        printf("torus:   u=%u v=%u psi=%u chi=%u rho=%u delta=%u sigma=%u\n",
               s.u,s.v,s.psi,s.chi,s.rho,s.delta,s.sigma);
        printf("C=%u H=%u hash=0x%016llX\n",
               k.c_q8,k.h_q8,(unsigned long long)k.hash);
        return 0;
    }
    fprintf(stderr,"Uso: %s [bbs|browse|bench|demo|calc|--help]\n",argv[0]);
    return 1;
}
