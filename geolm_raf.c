/* ============================================================
   geolm_raf.c  —  GeoLM v2 · RAFAELIA Baremetal · ARM32 NEON
   © 2026 Rafael Melo Reis — MIT License

   Integra logica de:
     raf_rafaelia_core  → ciclo ψχρΔΣΩ, PHI, SQRT3_2, R_CORR
     raf_rafaelia_math  → fibonacci_rafael, kahan_sum
     raf_rafaelia_vector→ raf_scalar_t, vec 1D-7D
     raf_toroid         → wrap toroidal, embedding 2D
     raf_geometry       → primitivos geo (toro, esfera)
     raf_bitstack       → hash CRC32, flip_count, coherence
     raf_zipraf         → save/load com CRC header (RAF2)
     raf_system         → identidade, camadas

   ARM32 NEON via arm_neon.h (intrinsics = asm inline 1:1)
   Zero deps externas. Arena estatica 64MB.

   ─────────────────────────────────────────────────────────
   COMPILAR + RODAR (Termux ARM32 Cortex-A7):

     mkdir -p ~/geolm
     cat > ~/geolm/geolm_raf.c << 'RAFEOF'
     [cole este arquivo]
     RAFEOF
     cd ~/geolm
     clang -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 \
           -mfloat-abi=softfp -std=c11 \
           geolm_raf.c -lm -o geolm_raf \
           && ./geolm_raf
   ============================================================ */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

/* ── tipos primitivos ─────────────────────────────────────── */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef float    f32;
typedef float    raf_scalar_t;
typedef double   f64;   /* tipo base RAFAELIA */

#define ALIGNED64  __attribute__((aligned(64)))
#define ALIGNED16  __attribute__((aligned(16)))
#define FI         __attribute__((always_inline)) static inline
#define NI         __attribute__((noinline))

/* ── constantes RAFAELIA (raf_rafaelia_core.h) ───────────── */
#define RAF_PHI       1.618033988749895f   /* φ razão áurea */
#define RAF_SQRT3_2   0.866025403784439f   /* √(3/2) espiral */
#define RAF_R_CORR    0.963999f            /* coerência residual */
#define RAF_PI        3.141592653589793f
#define RAF_SIN279  (-0.987688340595f)     /* sin(279°) */

/* ── identidade do sistema (raf_system.h) ─────────────────── */
static const char *RAF_ID      = "RAFAELIA-v2";
static const char *RAF_KERNEL  = "GeoLM-ARM32";
static const char *RAF_MODE    = "baremetal";
static const char *RAF_ETHIC   = "amor·coerencia·prova";

/* ── ciclo ψχρΔΣΩ (raf_rafaelia_core.c — raf_cycle_step) ── */
typedef struct {
    raf_scalar_t psi;    /* ψ intenção */
    raf_scalar_t chi;    /* χ observação */
    raf_scalar_t rho;    /* ρ ruído/resistência */
    raf_scalar_t delta;  /* Δ transmutação */
    raf_scalar_t sigma;  /* Σ memória */
    raf_scalar_t omega;  /* Ω completude */
    u64  iter;
} raf_cycle;

/* ciclo: ω = σ/φ  ψ = ω·√(3/2)  — do raf_cycle_step real */
FI raf_scalar_t raf_cycle_update(raf_cycle *c, raf_scalar_t in) {
    raf_scalar_t old_omega = c->omega;
    c->psi   = old_omega  * RAF_SQRT3_2;
    c->chi   = c->psi     * RAF_R_CORR;
    c->rho   = c->chi     - in;
    c->delta = c->rho < 0.f ? -c->rho : c->rho;
    c->sigma = c->sigma   * (1.f - 1.f/RAF_PHI)
             + c->delta   * (1.f/RAF_PHI);
    c->omega = c->sigma   / RAF_PHI;
    if(c->omega > 1.f) c->omega = 1.f;
    if(c->omega < 0.f) c->omega = 0.f;
    c->iter++;
    /* α ∈ [0.05, 0.35] */
    return 0.05f + 0.30f * c->omega;
}

/* raf_compute_fibonacci_rafael — raf_rafaelia_math.c formula 29 */
FI raf_scalar_t raf_fib_next(raf_scalar_t Fn) {
    return Fn * RAF_SQRT3_2 - RAF_PI * RAF_SIN279;
}

/* ── geometria primitiva (raf_geometry.h) ─────────────────── */
typedef struct { f32 x, y, z; } raf_vec3;
typedef enum { RAF_PRIM_NONE=0, RAF_PRIM_SPHERE=1,
               RAF_PRIM_CUBE=2, RAF_PRIM_TORUS=5 } raf_prim_type;
typedef struct {
    u8 type; raf_vec3 pos; raf_vec3 scale; f32 r;
} raf_prim;

/* ── toroide (raf_toroid.h — raf_toroid_wrap) ─────────────── */
FI u32 toro_wrap(i32 c, u32 W) {
    return (u32)(((c % (i32)W) + (i32)W) % (i32)W);
}
FI u32 toro_idx(i32 x, i32 y, u32 W, u32 H) {
    return toro_wrap(y,H)*W + toro_wrap(x,W);
}
/* distância toroidal 1D */
FI f32 toro_dist1d(i32 a, i32 b, u32 W) {
    i32 d = ((b - a) % (i32)W + (i32)W) % (i32)W;
    if(d > (i32)(W/2)) d = (i32)W - d;
    return (f32)d;
}

/* ── ANSI ─────────────────────────────────────────────────── */
#define CY  "\033[36m"
#define YE  "\033[33m"
#define GR  "\033[32m"
#define MA  "\033[35m"
#define RE  "\033[31m"
#define BO  "\033[1m"
#define RS  "\033[0m"

/* ── logo ASCII colorido ─────────────────────────────────── */
static void print_logo(void) {
    printf(
    CY BO
    "\n"
    "  ██████╗  █████╗ ███████╗ ┌─────────────────────┐\n"
    "  ██╔══██╗██╔══██╗██╔════╝ │" MA " RAFAELIA Baremetal " CY "│\n"
    "  ██████╔╝███████║█████╗   │" YE " ARM32 · NEON · φ   " CY "│\n"
    "  ██╔══██╗██╔══██║██╔══╝   │" GR " ψ·χ·ρ·Δ·Σ·Ω cycle  " CY "│\n"
    "  ██║  ██║██║  ██║██║      │" MA " GeoLM v2 · © 2026  " CY "│\n"
    "  ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝      └─────────────────────┘\n"
    RS MA
    "  id=" RS YE "RAFAELIA-v2" RS MA
    "  kernel=" RS YE "GeoLM-ARM32" RS MA
    "  ethic=" RS YE "amor·coerencia·prova" RS "\n\n"
    );
}

/* ── arena 64MB estático ──────────────────────────────────── */
#define ARENA_SZ (64u*1024u*1024u)
static u8 ALIGNED64 g_mem[ARENA_SZ];
typedef struct { u8 *b; u32 cap, bump; } Arena;
static Arena G;

static void arena_init(void) {
    G.b=g_mem; G.cap=ARENA_SZ; G.bump=0;
    memset(g_mem, 0, ARENA_SZ);
}
FI void *aa(u32 n, u32 al) {
    u32 m=al-1u, s=(G.bump+m)&~m, e=s+n;
    if(e > G.cap) return NULL;
    G.bump=e; return G.b+s;
}
#define A(T,n)    ((T*)aa((u32)(sizeof(T)*(n)), 16u))
#define A64(T,n)  ((T*)aa((u32)(sizeof(T)*(n)), 64u))

/* ── ARM32 NEON kernels ───────────────────────────────────── */
#ifdef __ARM_NEON__
#include <arm_neon.h>

/* dot(a,b,d) — vmla.f32 (multiply-accumulate 4-wide, 1 cycle Cortex-A7) */
FI f32 dot_neon(const f32 * restrict a, const f32 * restrict b, u32 d) {
    float32x4_t acc = vdupq_n_f32(0.f);
    u32 i=0;
    for(; i+3 < d; i+=4)
        acc = vmlaq_f32(acc, vld1q_f32(a+i), vld1q_f32(b+i));
    /* horizontal reduce: hadd → hadd */
    float32x2_t s = vadd_f32(vget_low_f32(acc), vget_high_f32(acc));
    s = vpadd_f32(s, s);
    f32 r = vget_lane_f32(s, 0);
    for(; i < d; i++) r += a[i]*b[i];
    return r;
}

/* y[i] += alpha*x[i] — vmlaq_f32 */
FI void saxpy_neon(f32 * restrict y, const f32 * restrict x,
                   f32 alpha, u32 d) {
    float32x4_t va = vdupq_n_f32(alpha);
    u32 i=0;
    for(; i+3 < d; i+=4)
        vst1q_f32(y+i, vmlaq_f32(vld1q_f32(y+i), vld1q_f32(x+i), va));
    for(; i < d; i++) y[i] += alpha*x[i];
}

/* dst[i] = a[i] + b[i] */
FI void vadd_neon(f32 * restrict dst,
                  const f32 * restrict a,
                  const f32 * restrict b, u32 d) {
    u32 i=0;
    for(; i+7 < d; i+=8) {
        vst1q_f32(dst+i,   vaddq_f32(vld1q_f32(a+i),   vld1q_f32(b+i)));
        vst1q_f32(dst+i+4, vaddq_f32(vld1q_f32(a+i+4), vld1q_f32(b+i+4)));
    }
    for(; i < d; i++) dst[i]=a[i]+b[i];
}

/* v[i] *= s */
FI void vscale_neon(f32 * restrict v, f32 s, u32 d) {
    float32x4_t vs = vdupq_n_f32(s);
    u32 i=0;
    for(; i+3 < d; i+=4) vst1q_f32(v+i, vmulq_f32(vld1q_f32(v+i), vs));
    for(; i < d; i++) v[i]*=s;
}

#define DOT(a,b,d)       dot_neon(a,b,d)
#define SAXPY(y,x,a,d)   saxpy_neon(y,x,a,d)
#define VADD(d,a,b,n)    vadd_neon(d,a,b,n)
#define VSCALE(v,s,n)    vscale_neon(v,s,n)
#define NEON_TAG         YE " [NEON]" RS

#else  /* fallback scalar — Cortex-A7 sem flag neon */

FI f32 dot_neon(const f32 *a, const f32 *b, u32 d) {
    f32 s=0; u32 i=0;
    for(;i+3<d;i+=4) s+=a[i]*b[i]+a[i+1]*b[i+1]
                       +a[i+2]*b[i+2]+a[i+3]*b[i+3];
    for(;i<d;i++) s+=a[i]*b[i];
    return s;
}
FI void saxpy_neon(f32 *y, const f32 *x, f32 a, u32 d)
    {for(u32 i=0;i<d;i++)y[i]+=a*x[i];}
FI void vadd_neon(f32 *dst, const f32 *a, const f32 *b, u32 d)
    {for(u32 i=0;i<d;i++)dst[i]=a[i]+b[i];}
FI void vscale_neon(f32 *v, f32 s, u32 d)
    {for(u32 i=0;i<d;i++)v[i]*=s;}

#define DOT(a,b,d)       dot_neon(a,b,d)
#define SAXPY(y,x,a,d)   saxpy_neon(y,x,a,d)
#define VADD(d,a,b,n)    vadd_neon(d,a,b,n)
#define VSCALE(v,s,n)    vscale_neon(v,s,n)
#define NEON_TAG         " [scalar]"

#endif /* __ARM_NEON__ */

/* ── math puro — sem libm no caminho quente ──────────────── */
FI f32 fexpf(f32 x) {
    if(x >  10.f) return 22026.f;
    if(x < -10.f) return 4.54e-5f;
    f32 x2=x*x, x3=x2*x, x4=x3*x;
    return 1.f+x+x2*.5f+x3*.16667f+x4*.04167f;
}
static void softmax(f32 *v, u32 n) {
    f32 mx=v[0];
    for(u32 i=1;i<n;i++) if(v[i]>mx) mx=v[i];
    f32 s=0;
    for(u32 i=0;i<n;i++) { v[i]=fexpf(v[i]-mx); s+=v[i]; }
    f32 inv = s>1e-9f ? 1.f/s : 0.f;
    for(u32 i=0;i<n;i++) v[i]*=inv;
}
/* Kahan summation — raf_sum_kahan (raf_rafaelia_math.c) */
FI f32 kahan_sum(const f32 *v, u32 n) {
    f32 sum=0.f, c=0.f;
    for(u32 i=0;i<n;i++) {
        f32 y=v[i]-c, t=sum+y;
        c=(t-sum)-y; sum=t;
    }
    return sum;
}
static void layer_norm(f32 *o, const f32 *x, u32 d) {
    f32 m = kahan_sum(x,d) / (f32)d;
    f32 vr=0;
    for(u32 i=0;i<d;i++) { f32 r=x[i]-m; vr+=r*r; }
    f32 is = 1.f/sqrtf(vr/(f32)d + 1e-5f);
    for(u32 i=0;i<d;i++) o[i]=(x[i]-m)*is;
}

/* ── bitstack CRC32 (raf_bitstack.c / raf_zipraf.c) ─────── */
typedef struct {
    u64  hash;
    u32  flips;
    u32  stable;
    f32  coherence;   /* R_corr dinâmico */
} BitStack;

static u32 crc32b(const void *buf, u32 len) {
    const u8 *p=(const u8*)buf;
    u32 crc=0xFFFFFFFFu;
    for(u32 i=0;i<len;i++) {
        crc ^= (u32)p[i];
        for(u32 b=0;b<8;b++) crc=(crc>>1)^(0xEDB88320u&-(crc&1u));
    }
    return ~crc;
}
static void bs_update(BitStack *bs, const f32 *w, u32 n) {
    u32 h=crc32b(w, n*sizeof(f32));
    u32 diff=h^(u32)bs->hash, fc=0;
    for(u32 t=diff;t;t>>=1) fc+=(t&1u);  /* popcount */
    bs->flips += fc;
    bs->stable = (fc < 16) ? bs->stable+1 : 0;
    bs->hash   = (u64)h;
    bs->coherence = RAF_R_CORR * (1.f - (f32)fc/256.f);
    if(bs->coherence < 0.1f) bs->coherence=0.1f;
}

/* ── vocab (hash table aberta, djb2) ─────────────────────── */
#define VMAX   8192
#define EDIM     64
#define SEQLEN   64
#define TW       91    /* cols da grade toroidal ~sqrt(VMAX) */
#define TH       90    /* linhas */

typedef struct { char s[32]; u32 id,freq; u8 ok; } VE;
typedef struct { VE *e; u32 sz,cap; } Vocab;

static Vocab *vocab_new(void) {
    Vocab *v=A(Vocab,1); if(!v) return NULL;
    v->e=A64(VE,VMAX);   if(!v->e) return NULL;
    memset(v->e,0,sizeof(VE)*VMAX);
    v->sz=4; v->cap=VMAX;
    const char *sp[]={"<pad>","<unk>","<bos>","<eos>",NULL};
    for(i32 i=0;sp[i];i++) {
        u32 h=5381u; const char *p=sp[i];
        while(*p) h=((h<<5)+h)^(u8)*p++;
        h%=VMAX;
        strncpy(v->e[h].s,sp[i],31);
        v->e[h].id=(u32)i; v->e[h].ok=1;
    }
    return v;
}
static u32 vins(Vocab *v, const char *s) {
    if(v->sz >= v->cap-1) return 1u;
    u32 h=5381u; const char *p=s;
    while(*p) h=((h<<5)+h)^(u8)*p++;
    h%=v->cap;
    for(u32 pr=0;pr<v->cap;pr++) {
        VE *e=&v->e[h];
        if(!e->ok) { strncpy(e->s,s,31); e->id=v->sz++; e->freq=1; e->ok=1; return e->id; }
        if(!strncmp(e->s,s,31)) { e->freq++; return e->id; }
        h=(h+1)%v->cap;
    }
    return 1u;
}
static u32 vget(const Vocab *v, const char *s) {
    u32 h=5381u; const char *p=s;
    while(*p) h=((h<<5)+h)^(u8)*p++;
    h%=v->cap;
    for(u32 pr=0;pr<v->cap;pr++) {
        const VE *e=&v->e[h];
        if(!e->ok) return 1u;
        if(!strncmp(e->s,s,31)) return e->id;
        h=(h+1)%v->cap;
    }
    return 1u;
}
static const char *vstr(const Vocab *v, u32 id) {
    for(u32 i=0;i<v->cap;i++) if(v->e[i].ok && v->e[i].id==id) return v->e[i].s;
    return "<unk>";
}

/* ── tokenizer ─────────────────────────────────────────────── */
static u32 tokenize(Vocab *v, const char *t, u32 len,
                    u32 *ids, u32 max) {
    char b[32]; u32 bp=0, n=0;
    #define EM() do{ if(bp>0 && n<max){ b[bp]=0; ids[n++]=vins(v,b); bp=0; } }while(0)
    for(u32 i=0;i<len && n<max;i++) {
        u8 c=(u8)t[i]; if(c>='A'&&c<='Z') c+=32u;
        if(c==' '||c=='\t'||c=='\n'||c=='\r') { EM(); continue; }
        if(c=='.'||c==','||c==':'||c=='!'||c=='?'||c==';') {
            EM();
            if(n<max) { b[0]=(char)c; b[1]=0; ids[n++]=vins(v,b); }
            continue;
        }
        if(bp<31) b[bp++]=(char)c;
        else { EM(); b[bp++]=(char)c; }
    }
    EM();
    #undef EM
    return n;
}

/* ── toroid embedding (raf_toroid.c — 2D com wrap) ──────── */
typedef struct {
    f32 ALIGNED64 W[VMAX][EDIM];    /* tabela embedding toroidal */
    f32 ALIGNED64 P[SEQLEN][EDIM];  /* pos encoding modulado φ */
    u32 vsz;
} TorEmbed;

static TorEmbed *temb_new(u32 vsz) {
    TorEmbed *e=A64(TorEmbed,1); if(!e) return NULL;
    e->vsz=vsz;
    u32 rng=0x52414641u;  /* semente "RAFA" */
    f32 sc=1.f/sqrtf((f32)EDIM);
    for(u32 v=0;v<vsz;v++) {
        /* posição toroidal do vocab id na grade TW×TH */
        i32 tx=(i32)toro_wrap((i32)v, TW);
        i32 ty=(i32)toro_wrap((i32)(v/TW), TH);
        for(u32 d=0;d<EDIM;d++) {
            rng=rng*1664525u+1013904223u;
            f32 r=((f32)(i32)rng)/2147483648.f*sc;
            /* modulação toroidal: distância ao centro da grade */
            f32 cx=toro_dist1d(tx,(i32)(TW/2),TW);
            f32 cy=toro_dist1d(ty,(i32)(TH/2),TH);
            f32 tmod=sinf(cx*RAF_PHI+(f32)d)*cosf(cy*RAF_PHI+(f32)d)*0.05f;
            e->W[v][d] = r + tmod*sc;
        }
    }
    /* pos encoding: seno/cos + modulação φ na fase */
    for(u32 pos=0;pos<SEQLEN;pos++) {
        f32 fn=(f32)(pos+1);
        for(u32 i=0;i<EDIM;i+=2) {
            f32 freq=1.f/powf(10000.f,(f32)i/(f32)EDIM);
            f32 phi_phase=RAF_PHI*(f32)(i/2+1)*0.01f;
            e->P[pos][i]   = sinf((f32)pos*freq + phi_phase);
            e->P[pos][i+1] = cosf((f32)pos*freq + phi_phase);
        }
        /* decaimento Rafaelia na última dimensão */
        fn=raf_fib_next(fn);
        f32 inv_fn = fabsf(fn) > 1e-4f ? 1.f/fabsf(fn) : 1.f;
        if(inv_fn>2.f) inv_fn=2.f;
        e->P[pos][EDIM-1] *= inv_fn;
    }
    return e;
}

/* ── modelo RAF bigram ────────────────────────────────────── */
typedef struct {
    f32 ALIGNED64 emb[VMAX][EDIM];  /* embedding treinável */
    f32 ALIGNED64 W  [EDIM][VMAX];  /* head: W[i][j] */
    f32 ALIGNED16 b  [VMAX];        /* bias */
    u32 vsz, step;
    f32 best_loss;
    raf_cycle  cycle;   /* ψχρΔΣΩ — controla α dinâmico */
    BitStack   bs;      /* integridade CRC32 */
    f32        raf_Fn;  /* valor corrente da sequência Rafaelia */
} RafModel;

static RafModel *rm_new(u32 vsz, const TorEmbed *te) {
    RafModel *m=A64(RafModel,1); if(!m) return NULL;
    m->vsz=vsz; m->step=0; m->best_loss=1e9f; m->raf_Fn=1.f;
    /* inicializa embedding a partir da grade toroidal */
    memcpy(m->emb, te->W, vsz*EDIM*sizeof(f32));
    /* head Xavier */
    u32 rng=0x52414648u; f32 sc=1.f/sqrtf((f32)EDIM);
    for(u32 i=0;i<EDIM;i++)
        for(u32 j=0;j<vsz;j++) {
            rng=rng*1664525u+1013904223u;
            m->W[i][j]=((f32)(i32)rng)/2147483648.f*sc;
        }
    memset(m->b, 0, vsz*sizeof(f32));
    memset(&m->cycle, 0, sizeof(m->cycle));
    memset(&m->bs,    0, sizeof(m->bs));
    m->bs.coherence=RAF_R_CORR;
    return m;
}

static f32 rm_step(RafModel *m, u32 ctx, u32 tgt,
                    f32 *logits, f32 lr_base) {
    if(ctx>=m->vsz || tgt>=m->vsz) return 0.f;
    f32 *emb=m->emb[ctx];

    /* forward: logit[j] = b[j] + Σ_i W[i][j]*emb[i] */
    memcpy(logits, m->b, m->vsz*sizeof(f32));
    for(u32 i=0;i<EDIM;i++)
        SAXPY(logits, m->W[i], emb[i], m->vsz);  /* NEON vmlaq */

    softmax(logits, m->vsz);
    f32 loss=-logf(logits[tgt]+1e-9f);

    /* α dinâmico via ciclo ψχρΔΣΩ */
    f32 alpha=raf_cycle_update(&m->cycle, loss);
    f32 lr=lr_base * alpha;

    /* decaimento via sequência Rafaelia */
    m->raf_Fn=raf_fib_next(m->raf_Fn);
    f32 decay=fabsf(m->raf_Fn) > 1e-3f ? 1.f/fabsf(m->raf_Fn) : 1.f;
    if(decay<0.1f) decay=0.1f;
    if(decay>2.0f) decay=2.0f;
    lr *= decay;

    /* backward */
    static f32 ALIGNED16 dL[VMAX];
    for(u32 j=0;j<m->vsz;j++) dL[j]=logits[j]-(j==tgt?1.f:0.f);

    /* ∂L/∂W[i][j] = emb[i]*dL[j]  → W[i] -= lr*emb[i]*dL */
    for(u32 i=0;i<EDIM;i++) SAXPY(m->W[i], dL, -lr*emb[i], m->vsz);

    /* ∂L/∂b = dL */
    SAXPY(m->b, dL, -lr, m->vsz);

    /* ∂L/∂emb[i] = Σ_j W[i][j]*dL[j] = DOT(W[i], dL) */
    for(u32 i=0;i<EDIM;i++)
        emb[i] -= lr * DOT(m->W[i], dL, m->vsz);

    m->step++;
    if(loss < m->best_loss) m->best_loss=loss;

    /* integridade bitstack a cada 100 steps */
    if(m->step%100==0)
        bs_update(&m->bs, &m->emb[0][0], m->vsz*EDIM);

    return loss;
}

/* ── save / load (raf_zipraf.c — cabeçalho RAF2 + CRC32) ── */
#define RAF2_MAGIC 0x52414632u   /* "RAF2" */
typedef struct {
    u32 magic, vsz, step, dim;
    f32 best;
    u32 crc_emb;
    u32 _pad[2];
} RAF2Header;

static void rm_save(const RafModel *m, const Vocab *v, const char *path) {
    FILE *fp=fopen(path,"wb"); if(!fp) { printf(RE "[save] erro: %s\n" RS,path); return; }
    u32 crc=crc32b(m->emb, m->vsz*EDIM*sizeof(f32));
    RAF2Header h={ RAF2_MAGIC, m->vsz, m->step, EDIM, m->best_loss, crc, {0,0} };
    fwrite(&h, sizeof(h), 1, fp);
    fwrite(m->emb, sizeof(f32), m->vsz*EDIM, fp);
    fwrite(m->W,   sizeof(f32), EDIM*m->vsz,  fp);
    fwrite(m->b,   sizeof(f32), m->vsz,        fp);
    fwrite(&v->sz, 4, 1, fp);
    fwrite(v->e,   sizeof(VE), v->cap, fp);
    fclose(fp);
    printf(GR "[save] %s  vsz=%u step=%u loss=%.4f crc=%08X\n" RS,
           path, m->vsz, m->step, m->best_loss, crc);
}

static i32 rm_load(RafModel *m, Vocab *v, const char *path) {
    FILE *fp=fopen(path,"rb"); if(!fp) return -1;
    RAF2Header h;
    fread(&h, sizeof(h), 1, fp);
    if(h.magic!=RAF2_MAGIC || h.dim!=EDIM) { fclose(fp); return -2; }
    m->vsz=h.vsz; m->step=h.step; m->best_loss=h.best;
    fread(m->emb, sizeof(f32), m->vsz*EDIM, fp);
    fread(m->W,   sizeof(f32), EDIM*m->vsz,  fp);
    fread(m->b,   sizeof(f32), m->vsz,        fp);
    u32 crc=crc32b(m->emb, m->vsz*EDIM*sizeof(f32));
    const char *integ = (crc==h.crc_emb) ? GR "OK" RS : YE "DIFF" RS;
    printf(GR "[load] %s  vsz=%u step=%u loss=%.4f crc=%s\n" RS,
           path, m->vsz, m->step, m->best_loss, integ);
    fread(&v->sz, 4, 1, fp);
    fread(v->e,   sizeof(VE), v->cap, fp);
    fclose(fp);
    return 0;
}

/* ── treino ───────────────────────────────────────────────── */
static void train(RafModel *m, const u32 *ids, u32 n,
                  u32 epochs, f32 lr) {
    static f32 ALIGNED16 lg[VMAX];
    printf(BO "[train] tokens=%u epochs=%u lr=%.5f vsz=%u\n" RS,
           n, epochs, lr, m->vsz);
    struct timespec t0,t1;
    clock_gettime(CLOCK_MONOTONIC,&t0);
    for(u32 ep=0;ep<epochs;ep++) {
        f32 tot=0; u32 cnt=0;
        for(u32 i=0;i+1<n;i++) { tot+=rm_step(m,ids[i],ids[i+1],lg,lr); cnt++; }
        f32 ls=tot/(f32)(cnt?cnt:1u);
        if(ep%5==0 || ep==epochs-1) {
            clock_gettime(CLOCK_MONOTONIC,&t1);
            f64 el=(t1.tv_sec-t0.tv_sec)+(t1.tv_nsec-t0.tv_nsec)*1e-9;
            printf("  ep=%3u loss=" YE "%.4f" RS
                   "  α=%.3f  ω=%.3f  coh=%.3f  %.1fs\n",
                   ep, ls,
                   0.05f+0.30f*m->cycle.omega,
                   m->cycle.omega,
                   m->bs.coherence, el);
        }
    }
}

/* ── geração com temperatura + sampling multinomial ─────── */
static void generate(const RafModel *m, const Vocab *v,
                     u32 start, u32 ng, f32 temp) {
    static f32 ALIGNED16 lg[VMAX];
    u32 cur=start;
    u32 rng=(u32)time(NULL);
    printf(GR "[>] " RS);
    for(u32 i=0;i<ng;i++) {
        /* logit[j] = b[j] + Σ_i W[i][j]*emb[cur][i]  (NEON) */
        memcpy(lg, m->b, m->vsz*sizeof(f32));
        for(u32 ii=0;ii<EDIM;ii++)
            SAXPY(lg, m->W[ii], m->emb[cur][ii], m->vsz);
        VSCALE(lg, 1.f/temp, m->vsz);
        softmax(lg, m->vsz);
        /* sampling multinomial */
        rng=rng*1664525u+1013904223u;
        f32 r=((f32)(rng>>1))/(f32)0x7FFFFFFFu;
        f32 acc=0; u32 sam=0;
        for(u32 j=0;j<m->vsz;j++) { acc+=lg[j]; if(acc>=r){sam=j;break;} }
        printf("%s ", vstr(v,sam));
        cur=sam;
        if(cur==3u) break;  /* <eos> */
    }
    printf("\n");
}

/* ── status ciclo ψχρΔΣΩ ─────────────────────────────────── */
static void raf_status(const RafModel *m) {
    printf(MA
        "  ψ=%.3f  χ=%.3f  ρ=%.3f\n"
        "  Δ=%.3f  Σ=%.3f  Ω=%.3f  iter=%llu\n"
        "  Fn=%.5f  flips=%u  stable=%u  coh=%.3f\n"
        "  steps=%u  best_loss=%.4f\n"
        RS,
        m->cycle.psi,   m->cycle.chi,   m->cycle.rho,
        m->cycle.delta, m->cycle.sigma, m->cycle.omega,
        (unsigned long long)m->cycle.iter,
        m->raf_Fn, m->bs.flips, m->bs.stable, m->bs.coherence,
        m->step, m->best_loss);
}

/* ── REPL ─────────────────────────────────────────────────── */
static void help(void) {
    printf(BO CY "\n  Comandos GeoLM RAF:\n" RS);
    const char *cmds[][2]={
        {"train [ep] [lr]",    "treina (default: 30 epocas, lr=0.05)"},
        {"gen <palavra> [n]",  "gera N tokens (default 20)"},
        {"vocab [n]",          "lista N tokens do vocabulario"},
        {"status",             "ciclo ψχρΔΣΩ + bitstack"},
        {"save <arquivo>",     "salva pesos + vocab (RAF2+CRC32)"},
        {"load <arquivo>",     "carrega pesos + vocab"},
        {"bench",              "benchmark NEON dot 1M iters"},
        {"help",               "esta ajuda"},
        {"quit",               "sair"},
        {NULL,NULL}
    };
    for(u32 i=0;cmds[i][0];i++)
        printf("    " YE "%-24s" RS " %s\n", cmds[i][0], cmds[i][1]);
    printf("\n");
}

/* ── fetch HTTP (wget/curl via popen) ────────────────────── */
#define TBUF (2*1024*1024)
static char g_tbuf[TBUF];

static u32 fetch_url(const char *url, Vocab *v, u32 *ids, u32 max) {
    char cmd[512];
    snprintf(cmd,sizeof(cmd),
             "wget -q --timeout=15 -O - \"%s\" 2>/dev/null", url);
    FILE *fp=popen(cmd,"r");
    if(!fp) {
        snprintf(cmd,sizeof(cmd),
                 "curl -s --max-time 15 \"%s\" 2>/dev/null", url);
        fp=popen(cmd,"r");
        if(!fp) return 0;
    }
    u32 got=0; size_t r;
    while(got<TBUF-1 && (r=fread(g_tbuf+got,1,4096,fp))>0) got+=(u32)r;
    g_tbuf[got]=0; pclose(fp);
    if(!got) return 0;
    return tokenize(v,g_tbuf,got,ids,max);
}

/* ════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════ */
int main(int argc, char **argv) {
    arena_init();
    print_logo();
    printf("  id=%s  kernel=%s  mode=%s\n\n",
           RAF_ID, RAF_KERNEL, RAF_MODE);

    Vocab *V=vocab_new(); if(!V) { puts("[FAIL] vocab"); return 1; }

    static u32 ALIGNED16 ids[65536];
    u32 nids=0;

    /* texto semente embutido */
    const char *seed=
        "no principio era o verbo e o verbo estava com deus "
        "o verbo era deus pela fe entendemos que os mundos foram criados "
        "pela palavra de deus o senhor e o meu pastor nada me faltara "
        "a sabedoria clama nas ruas o temor do senhor e o principio "
        "da sabedoria conhecereis a verdade e a verdade vos libertara "
        "eu sou o caminho a verdade e a vida o amor nao falha "
        "a fe move montanhas a esperanca nao decepciona "
        "quem tem ouvidos para ouvir que oucva o espirito de deus "
        "pairava sobre as aguas e deus disse haja luz e houve luz";
    nids=tokenize(V,(char*)seed,(u32)strlen(seed),ids,65536);

    /* argumento opcional: arquivo de texto ou fetch */
    if(argc>=2) {
        if(!strcmp(argv[1],"fetch")) {
            const char *urls[]={
                "https://www.gutenberg.org/cache/epub/10/pg10.txt",
                "https://www.gutenberg.org/cache/epub/216/pg216.txt",
                NULL
            };
            for(u32 u=0; urls[u] && nids<60000; u++) {
                printf("[fetch] %s\n",urls[u]);
                u32 got=fetch_url(urls[u],V,ids+nids,65536-nids);
                nids+=got;
                printf("  +%u tokens (total=%u vsz=%u)\n",got,nids,V->sz);
            }
        } else {
            FILE *fp=fopen(argv[1],"rb");
            if(fp) {
                size_t r=fread(g_tbuf,1,TBUF-1,fp); g_tbuf[r]=0; fclose(fp);
                nids=tokenize(V,g_tbuf,(u32)r,ids,65536);
                printf("[load] %s → %u tokens vsz=%u\n",argv[1],nids,V->sz);
            }
        }
    }

    /* inicializa embedding toroidal e modelo */
    printf("[init] toroid embed TW=%d TH=%d  vsz=%u\n",TW,TH,V->sz);
    TorEmbed *te=temb_new(V->sz); if(!te) { puts("[FAIL] emb"); return 1; }
    RafModel *M=rm_new(V->sz,te); if(!M) { puts("[FAIL] model"); return 1; }

    /* warm-up inicial */
    printf("[warmup] " MA "ψχρΔΣΩ" RS " seed=%u tok — ethic: %s\n",
           nids, RAF_ETHIC);
    train(M,ids,nids,20,0.05f);
    raf_status(M);

    help();

    /* REPL */
    char line[256];

    for(;;) {
        printf(BO CY "raf> " RS); fflush(stdout);
        if(!fgets(line,sizeof(line),stdin)) break;
        line[strcspn(line,"\n")]=0;
        if(!line[0]) continue;

        char cmd[64], a1[64], a2[64];
        cmd[0]=a1[0]=a2[0]=0;
        sscanf(line,"%63s %63s %63s",cmd,a1,a2);

        if(!strcmp(cmd,"quit") || !strcmp(cmd,"exit")) break;

        else if(!strcmp(cmd,"help")) help();

        else if(!strcmp(cmd,"status")) raf_status(M);

        else if(!strcmp(cmd,"vocab")) {
            u32 lim=a1[0]?(u32)atoi(a1):20u;
            printf("  vsz=%u\n",V->sz);
            u32 sh=0;
            for(u32 i=0;i<V->cap && sh<lim;i++)
                if(V->e[i].ok && V->e[i].id>3u)
                    { printf("  %4u: %-16s freq=%u\n",
                             V->e[i].id, V->e[i].s, V->e[i].freq); sh++; }
        }

        else if(!strcmp(cmd,"gen")) {
            u32 st=2u, ng=20u;
            if(a1[0]) { st=vget(V,a1); if(st==1u){printf("  nao encontrado, usando <bos>\n");st=2u;} }
            if(a2[0]) ng=(u32)atoi(a2);
            generate(M,V,st,ng,0.8f);
        }

        else if(!strcmp(cmd,"train")) {
            u32 ep=a1[0]?(u32)atoi(a1):20u;
            f32 lr=a2[0]?(f32)atof(a2):0.05f;
            train(M,ids,nids,ep,lr);
            raf_status(M);
        }

        else if(!strcmp(cmd,"save") && a1[0]) rm_save(M,V,a1);

        else if(!strcmp(cmd,"load") && a1[0]) {
            RafModel *m2=A64(RafModel,1);
            if(m2 && rm_load(m2,V,a1)==0) M=m2;
        }

        else if(!strcmp(cmd,"bench")) {
            static f32 ALIGNED16 va[EDIM], vb[EDIM];
            for(u32 i=0;i<EDIM;i++) { va[i]=(f32)i; vb[i]=1.f/(f32)(i+1u); }
            struct timespec t0,t1;
            clock_gettime(CLOCK_MONOTONIC,&t0);
            volatile f32 sink=0;
            for(u32 i=0;i<1000000u;i++) sink+=DOT(va,vb,EDIM);
            clock_gettime(CLOCK_MONOTONIC,&t1);
            f64 ms=(t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)*1e-6;
            printf("  1M × dot[%d]  %.2f ms  → " YE "%.0f Mdot/s" RS " %s\n",
                   EDIM, ms, 1000.0/ms, NEON_TAG);
            (void)sink;
        }

        else printf(YE "  desconhecido: %s\n" RS, cmd);
    }

    printf(GR "  ψ·χ·ρ·Δ·Σ·Ω  Ω=%.4f  steps=%u  loss=%.4f\n" RS,
           M->cycle.omega, M->step, M->best_loss);
    return 0;
}
