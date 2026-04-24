/* ============================================================
   geolm.c — GeoLM ARM32: Language Model Geométrico Vetorial
   Sistema unificado: tipos, arena, vocab, tokenizer,
   embeddings, MHA, FFN, transformer, loss, SGD,
   ingestão (texto/JSON/PPM), CLI REPL, save/load,
   benchmark com medianas, métricas, ASCII art logo.

   Compilar no Termux ARM32:
     clang -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 \
           -mfloat-abi=softfp -std=c11 \
           geolm.c -lm -o geolm && ./geolm help

   Compilar em x86/x64 (dev/debug):
     gcc -O2 -std=c11 geolm.c -lm -o geolm && ./geolm help
   ============================================================ */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* ============================================================
   TIPOS PRIMITIVOS
   ============================================================ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;

/* ============================================================
   CONSTANTES DO SISTEMA
   ============================================================ */
#define VOCAB_MAX      8192
#define TOKEN_LEN_MAX    32
#define EMBED_DIM        64
#define SEQ_LEN          64
#define HIDDEN_DIM      128
#define N_HEADS            4
#define HEAD_DIM          16   /* EMBED_DIM / N_HEADS */
#define N_LAYERS           2
#define ARENA_MB          64
#define ARENA_BYTES        (ARENA_MB * 1024 * 1024)
#define TEXT_BUF_SIZE      (2 * 1024 * 1024)
#define IDS_MAX            65536
#define MODEL_MAGIC        0x474C4D32u   /* "GLM2" */
#define BENCH_RUNS         64
#define PPM_MAX_PIXELS     (512 * 512)

#define ALIGNED64  __attribute__((aligned(64)))
#define ALIGNED16  __attribute__((aligned(16)))
#define FORCE_INLINE __attribute__((always_inline)) static inline
#define NO_INLINE    __attribute__((noinline))

/* ============================================================
   ANSI CORES
   ============================================================ */
#define CYN  "\033[36m"
#define YEL  "\033[33m"
#define GRN  "\033[32m"
#define RED  "\033[31m"
#define MAG  "\033[35m"
#define BLU  "\033[34m"
#define WHT  "\033[37m"
#define BOLD "\033[1m"
#define DIM  "\033[2m"
#define RST  "\033[0m"
#define CLR  "\033[2J\033[H"

/* ============================================================
   ASCII ART LOGO
   ============================================================ */
static void print_logo(void) {
    printf(
        "\n"
        BOLD CYN "   ██████╗ ███████╗ ██████╗ " MAG "██╗     ███╗   ███╗\n" RST
        BOLD CYN "  ██╔════╝ ██╔════╝██╔═══██╗" MAG "██║     ████╗ ████║\n" RST
        BOLD CYN "  ██║  ███╗█████╗  ██║   ██║" MAG "██║     ██╔████╔██║\n" RST
        BOLD CYN "  ██║   ██║██╔══╝  ██║   ██║" MAG "██║     ██║╚██╔╝██║\n" RST
        BOLD CYN "  ╚██████╔╝███████╗╚██████╔╝" MAG "███████╗██║ ╚═╝ ██║\n" RST
        BOLD CYN "   ╚═════╝ ╚══════╝ ╚═════╝ " MAG "╚══════╝╚═╝     ╚═╝\n" RST
        "\n"
        DIM WHT "  " BLU "▸ " RST DIM "ARM32 · Toroidal Geometry · Semantic Attractors\n" RST
        DIM WHT "  " BLU "▸ " RST DIM "Invariante Geométrica · Multifilamento · Fractal\n" RST
        DIM WHT "  " BLU "▸ " RST DIM "Entropia · Coerência · Prova Matemática\n\n" RST
        BLU "  ┌─────────────────────────────────────────────┐\n" RST
        BLU "  │" RST BOLD YEL "  Ω" RST " = conhecimento que sustenta forma sob pressão" BLU "  │\n" RST
        BLU "  └─────────────────────────────────────────────┘\n\n" RST
    );
}

static void print_separator(const char *label) {
    printf(BLU "\n  ─────────── " YEL "%s" BLU " ───────────\n\n" RST, label);
}

/* ============================================================
   ARENA DE MEMÓRIA — zero malloc no caminho quente
   ============================================================ */
typedef struct {
    u8  *base;
    u32  cap;
    u32  bump;
    u32  n_alloc;
    u32  peak;
} Arena;

static u8 ALIGNED64 g_arena_buf[ARENA_BYTES];
static Arena g_arena;

static void arena_init(Arena *a) {
    a->base    = g_arena_buf;
    a->cap     = ARENA_BYTES;
    a->bump    = 0;
    a->n_alloc = 0;
    a->peak    = 0;
    memset(g_arena_buf, 0, ARENA_BYTES);
}

FORCE_INLINE void *arena_alloc(Arena *a, u32 n, u32 align) {
    u32 mask  = align - 1u;
    u32 start = (a->bump + mask) & ~mask;
    u32 end   = start + n;
    if (end > a->cap) return NULL;
    a->bump = end;
    a->n_alloc++;
    if (end > a->peak) a->peak = end;
    return a->base + start;
}

FORCE_INLINE u32  arena_save(const Arena *a)       { return a->bump; }
FORCE_INLINE void arena_restore(Arena *a, u32 mark){ a->bump = mark; }

#define ALLOC(T,n)   ((T*)arena_alloc(&g_arena,(u32)(sizeof(T)*(n)),16u))
#define ALLOC64(T,n) ((T*)arena_alloc(&g_arena,(u32)(sizeof(T)*(n)),64u))

static void arena_print_stats(const Arena *a, const char *label) {
    f32 pct = 100.f * (f32)a->bump / (f32)a->cap;
    printf("  " YEL "%-18s" RST " used=%uKB peak=%uKB cap=%uMB allocs=%u (%.1f%%)\n",
           label ? label : "arena",
           a->bump/1024, a->peak/1024, a->cap/(1024*1024),
           a->n_alloc, pct);
}

/* ============================================================
   MATH PRIMITIVO — sem libm no caminho quente
   ============================================================ */
FORCE_INLINE f32 fast_expf(f32 x) {
    if (x >  10.0f) return 22026.0f;
    if (x < -10.0f) return 0.0000454f;
    f32 x2=x*x, x3=x2*x, x4=x3*x;
    return 1.0f + x + x2*0.5f + x3*0.16667f + x4*0.04167f;
}

FORCE_INLINE f32 reluf(f32 x) { return x > 0.0f ? x : 0.0f; }

FORCE_INLINE f32 dot_f32(const f32 * restrict a,
                          const f32 * restrict b, u32 dim) {
    f32 s0=0,s1=0,s2=0,s3=0; u32 i=0;
    for(; i+3<dim; i+=4) {
        s0+=a[i]*b[i]; s1+=a[i+1]*b[i+1];
        s2+=a[i+2]*b[i+2]; s3+=a[i+3]*b[i+3];
    }
    for(; i<dim; i++) s0+=a[i]*b[i];
    return s0+s1+s2+s3;
}

FORCE_INLINE void vec_zero(f32 *v, u32 dim) {
    memset(v, 0, dim*sizeof(f32));
}

static void softmax_f32(f32 *v, u32 len) {
    f32 mx=v[0];
    for(u32 i=1;i<len;i++) if(v[i]>mx) mx=v[i];
    f32 sum=0;
    for(u32 i=0;i<len;i++) { v[i]=fast_expf(v[i]-mx); sum+=v[i]; }
    f32 inv = sum>1e-9f ? 1.0f/sum : 0.0f;
    for(u32 i=0;i<len;i++) v[i]*=inv;
}

static void layer_norm(f32 * restrict out,
                        const f32 * restrict x,
                        const f32 * restrict gamma,
                        const f32 * restrict beta,
                        u32 dim) {
    f32 mean=0, var=0;
    for(u32 i=0;i<dim;i++) mean+=x[i];
    mean /= (f32)dim;
    for(u32 i=0;i<dim;i++) { f32 d=x[i]-mean; var+=d*d; }
    var /= (f32)dim;
    f32 inv_std = 1.0f/sqrtf(var+1e-5f);
    for(u32 i=0;i<dim;i++) out[i]=(x[i]-mean)*inv_std*gamma[i]+beta[i];
}

/* matmul: out[seq][odim] = in[seq][idim] * W[idim][odim] */
static void matmul_seq(f32 * restrict out,
                        const f32 * restrict in,
                        const f32 * restrict W,
                        u32 seq_len, u32 idim, u32 odim) {
    for(u32 s=0; s<seq_len; s++) {
        const f32 *ri = in + s*idim;
        f32 *ro       = out + s*odim;
        vec_zero(ro, odim);
        for(u32 i=0;i<idim;i++) {
            const f32 *Wr=W+i*odim; f32 xi=ri[i]; u32 j=0;
            for(;j+3<odim;j+=4){
                ro[j]+=xi*Wr[j]; ro[j+1]+=xi*Wr[j+1];
                ro[j+2]+=xi*Wr[j+2]; ro[j+3]+=xi*Wr[j+3];
            }
            for(;j<odim;j++) ro[j]+=xi*Wr[j];
        }
    }
}

/* ============================================================
   VOCABULÁRIO — hash aberta, sem strdup
   ============================================================ */
typedef struct {
    char str[TOKEN_LEN_MAX];
    u32  id;
    u32  freq;
    u8   used;
    u8   _pad[3];
} VocabEntry;

typedef struct {
    VocabEntry *entries;
    u32 size, cap;
    u32 id_pad, id_unk, id_bos, id_eos;
} Vocab;

FORCE_INLINE u32 vocab_hash(const char *s, u32 cap) {
    u32 h=5381u;
    while(*s) h=((h<<5)+h)^(u8)*s++;
    return h % cap;
}

static Vocab *vocab_new(void) {
    Vocab *v = ALLOC(Vocab,1);
    if(!v) return NULL;
    v->entries = ALLOC64(VocabEntry, VOCAB_MAX);
    if(!v->entries) return NULL;
    memset(v->entries, 0, sizeof(VocabEntry)*VOCAB_MAX);
    v->size=4; v->cap=VOCAB_MAX;
    v->id_pad=0; v->id_unk=1; v->id_bos=2; v->id_eos=3;
    const char *sp[]={"<pad>","<unk>","<bos>","<eos>",NULL};
    for(i32 i=0;sp[i];i++){
        u32 h=vocab_hash(sp[i],VOCAB_MAX);
        strncpy(v->entries[h].str,sp[i],TOKEN_LEN_MAX-1);
        v->entries[h].id=(u32)i; v->entries[h].used=1;
    }
    return v;
}

static u32 vocab_insert(Vocab *v, const char *s) {
    if(v->size>=v->cap-1) return v->id_unk;
    u32 h=vocab_hash(s,v->cap);
    for(u32 p=0;p<v->cap;p++){
        VocabEntry *e=&v->entries[h];
        if(!e->used){
            strncpy(e->str,s,TOKEN_LEN_MAX-1);
            e->id=v->size++; e->freq=1; e->used=1;
            return e->id;
        }
        if(strncmp(e->str,s,TOKEN_LEN_MAX)==0){e->freq++;return e->id;}
        h=(h+1)%v->cap;
    }
    return v->id_unk;
}

static u32 vocab_lookup(const Vocab *v, const char *s) {
    u32 h=vocab_hash(s,v->cap);
    for(u32 p=0;p<v->cap;p++){
        const VocabEntry *e=&v->entries[h];
        if(!e->used) return v->id_unk;
        if(strncmp(e->str,s,TOKEN_LEN_MAX)==0) return e->id;
        h=(h+1)%v->cap;
    }
    return v->id_unk;
}

static const char *vocab_str(const Vocab *v, u32 id) {
    for(u32 i=0;i<v->cap;i++)
        if(v->entries[i].used && v->entries[i].id==id)
            return v->entries[i].str;
    return "<unk>";
}

static void vocab_top_n(const Vocab *v, u32 n) {
    /* exibe os N tokens mais frequentes */
    typedef struct { u32 id; u32 freq; } TF;
    static TF tf[VOCAB_MAX]; u32 cnt=0;
    for(u32 i=0;i<v->cap;i++)
        if(v->entries[i].used && v->entries[i].id>3)
            tf[cnt++]=(TF){v->entries[i].id,v->entries[i].freq};
    /* bubble sort parcial — só n primeiros */
    for(u32 i=0;i<n&&i<cnt;i++)
        for(u32 j=i+1;j<cnt;j++)
            if(tf[j].freq>tf[i].freq){TF t=tf[i];tf[i]=tf[j];tf[j]=t;}
    printf("  " BOLD "Top %u tokens:\n" RST, n<cnt?n:cnt);
    for(u32 i=0;i<n&&i<cnt;i++)
        printf("    " YEL "%4u" RST "  " GRN "%-20s" RST "  freq=%u\n",
               tf[i].id, vocab_str(v,tf[i].id), tf[i].freq);
}

/* ============================================================
   TOKENIZADOR — whitespace split + lowercase + pontuação
   ============================================================ */
FORCE_INLINE u8 tok_norm(u8 c){
    return (c>='A'&&c<='Z') ? c+32u : c;
}
FORCE_INLINE i32 tok_sep(u8 c){
    return c==' '||c=='\t'||c=='\n'||c=='\r';
}
FORCE_INLINE i32 tok_punct(u8 c){
    return c=='.'||c==','||c==';'||c==':'||c=='!'||c=='?'||
           c=='"'||c=='\''||c=='('||c==')'||c=='['||c==']';
}

static u32 tokenize(Vocab *v, const char *text, u32 len,
                    u32 *ids, u32 max_ids) {
    char buf[TOKEN_LEN_MAX]; u32 bp=0, n=0;
    #define EMIT() do { \
        if(bp>0&&n<max_ids){buf[bp]=0;ids[n++]=vocab_insert(v,buf);bp=0;} \
    } while(0)
    for(u32 i=0;i<len&&n<max_ids;i++){
        u8 c=tok_norm((u8)text[i]);
        if(tok_sep(c)){EMIT();continue;}
        if(tok_punct(c)){
            EMIT();
            if(n<max_ids){buf[0]=(char)c;buf[1]=0;ids[n++]=vocab_insert(v,buf);}
            continue;
        }
        if(bp<TOKEN_LEN_MAX-1) buf[bp++]=(char)c;
        else { EMIT(); buf[bp++]=(char)c; }
    }
    EMIT();
    #undef EMIT
    return n;
}

/* ============================================================
   INGESTÃO JSON — extrai campos text/content/body
   ============================================================ */
static u32 json_extract_text(const char *json, u32 jlen,
                              char *out, u32 out_size) {
    u32 written=0;
    const char *p=json, *end=json+jlen;
    const char *keys[]={"\"text\":","\"content\":","\"body\":","\"prompt\":",
                        "\"response\":","\"value\":",NULL};
    while(p<end && written<out_size-1){
        const char *found=NULL; u32 klen=0;
        for(u32 k=0;keys[k];k++){
            const char *f=strstr(p,keys[k]);
            if(f&&(!found||f<found)){found=f;klen=(u32)strlen(keys[k]);}
        }
        if(!found) break;
        p=found+klen;
        while(p<end&&(*p==' '||*p=='\t'||*p=='\n'))p++;
        if(p>=end||*p!='"'){p++;continue;}
        p++;
        while(p<end&&*p!='"'&&written<out_size-1){
            if(*p=='\\'){p++;
                if(p<end){
                    if(*p=='n'){out[written++]='\n';}
                    else if(*p=='t'){out[written++]='\t';}
                    else out[written++]=*p;
                    p++;continue;
                }
            }
            out[written++]=*p++;
        }
        out[written++]=' ';
        if(p<end)p++;
    }
    if(written<out_size) out[written]=0;
    return written;
}

/* ============================================================
   INGESTÃO PPM — aprende histogramas de cor como tokens
   Formato: P6 (binário RGB) ou P3 (texto)
   ============================================================ */
typedef struct {
    u8 r,g,b;
} RGB;

static u32 ppm_to_tokens(Vocab *v, const u8 *data, u32 size,
                          u32 *ids, u32 max_ids) {
    /* parse cabeçalho PPM P6 */
    if(size<7) return 0;
    if(data[0]!='P'||(data[1]!='6'&&data[1]!='3')) return 0;

    char hdr[128]; u32 hi=0;
    const u8 *p=data+2;
    const u8 *end=data+size;

    /* pula comentários e lê W H maxval */
    u32 W=0,H=0,maxv=0; i32 state=0;
    while(p<end && state<3){
        while(p<end&&(*p==' '||*p=='\t'||*p=='\n'||*p=='\r'))p++;
        if(p<end&&*p=='#'){while(p<end&&*p!='\n')p++;continue;}
        hi=0;
        while(p<end&&*p>='0'&&*p<='9'&&hi<120)hdr[hi++]=(char)*p++;
        hdr[hi]=0;
        if(state==0)W=(u32)atoi(hdr);
        else if(state==1)H=(u32)atoi(hdr);
        else maxv=(u32)atoi(hdr);
        state++;
    }
    if(p<end&&(*p=='\n'||*p==' '||*p=='\r'))p++;
    if(W==0||H==0||maxv==0) return 0;

    u32 n=0;
    u32 bpp = (data[1]=='6') ? 3u : 0u;
    u32 total_pixels = W*H;
    if(total_pixels > PPM_MAX_PIXELS) total_pixels=PPM_MAX_PIXELS;

    /* quantiza cada pixel em 1 de 8 regiões de cor (3 bits) */
    /* mapeia par de pixels em token "img_RRGG" */
    char tbuf[16];
    for(u32 i=0;i<total_pixels&&n<max_ids;i++){
        u8 r,g,b;
        if(bpp==3){
            if(p+2>=end)break;
            r=p[0];g=p[1];b=p[2];p+=3;
        } else {
            /* P3: lê 3 inteiros */
            r=g=b=0;
            for(u32 ch=0;ch<3;ch++){
                while(p<end&&(*p<'0'||*p>'9'))p++;
                u32 val=0;
                while(p<end&&*p>='0'&&*p<='9')val=val*10+(*p++)-'0';
                if(ch==0)r=(u8)(val&0xFF);
                else if(ch==1)g=(u8)(val&0xFF);
                else b=(u8)(val&0xFF);
            }
        }
        /* quantiza: 3 bits por canal -> 1 de 8 -> compõe token */
        u8 rq=r>>5, gq=g>>5, bq=b>>5;
        snprintf(tbuf,sizeof(tbuf),"img_%d%d%d",rq,gq,bq);
        if(n<max_ids) ids[n++]=vocab_insert(v,tbuf);
    }

    /* adiciona token de início e fim de imagem */
    if(n>0&&n<max_ids){
        snprintf(tbuf,sizeof(tbuf),"img_end");
        ids[n++]=vocab_insert(v,tbuf);
    }
    return n;
}

/* ============================================================
   MODELO COMPLETO — embeddings, transformer, lm_head
   ============================================================ */
typedef struct {
    f32 ALIGNED64 embed[VOCAB_MAX][EMBED_DIM];
    f32 ALIGNED64 pos_enc[SEQ_LEN][EMBED_DIM];
    /* transformer layer 0 */
    f32 ALIGNED64 Wq0[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 Wk0[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 Wv0[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 Wo0[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 W1_0[EMBED_DIM][HIDDEN_DIM];
    f32 ALIGNED64 W2_0[HIDDEN_DIM][EMBED_DIM];
    f32 ALIGNED16 b1_0[HIDDEN_DIM], b2_0[EMBED_DIM];
    f32 ALIGNED16 ln1a_0[EMBED_DIM], ln1b_0[EMBED_DIM];
    f32 ALIGNED16 ln2a_0[EMBED_DIM], ln2b_0[EMBED_DIM];
    /* transformer layer 1 */
    f32 ALIGNED64 Wq1[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 Wk1[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 Wv1[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 Wo1[EMBED_DIM][EMBED_DIM];
    f32 ALIGNED64 W1_1[EMBED_DIM][HIDDEN_DIM];
    f32 ALIGNED64 W2_1[HIDDEN_DIM][EMBED_DIM];
    f32 ALIGNED16 b1_1[HIDDEN_DIM], b2_1[EMBED_DIM];
    f32 ALIGNED16 ln1a_1[EMBED_DIM], ln1b_1[EMBED_DIM];
    f32 ALIGNED16 ln2a_1[EMBED_DIM], ln2b_1[EMBED_DIM];
    /* head */
    f32 ALIGNED64 head_W[EMBED_DIM][VOCAB_MAX];
    f32 ALIGNED16 head_b[VOCAB_MAX];
    u32 vocab_size;
    u32 step;
    f32 best_loss;
} Model;

/* buffers de trabalho — estáticos, fora do modelo */
static f32 ALIGNED16 g_x[SEQ_LEN][EMBED_DIM];
static f32 ALIGNED16 g_tmp[SEQ_LEN][EMBED_DIM];
static f32 ALIGNED16 g_tmp2[SEQ_LEN][EMBED_DIM];
static f32 ALIGNED16 g_Q[SEQ_LEN][EMBED_DIM];
static f32 ALIGNED16 g_K[SEQ_LEN][EMBED_DIM];
static f32 ALIGNED16 g_V[SEQ_LEN][EMBED_DIM];
static f32 ALIGNED16 g_scores[SEQ_LEN][SEQ_LEN];
static f32 ALIGNED16 g_logits[VOCAB_MAX];
static f32 ALIGNED16 g_hidden[SEQ_LEN][HIDDEN_DIM];

static u32 rng_state = 0x52414641u;  /* "RAFA" */
FORCE_INLINE f32 rng_f32(void) {
    rng_state = rng_state*1664525u+1013904223u;
    return ((f32)(i32)rng_state)/2147483648.0f;
}

static void model_init_weights(Model *m, u32 vocab_size) {
    m->vocab_size = vocab_size;
    m->step = 0;
    m->best_loss = 1e9f;

    /* embeddings Xavier */
    f32 se = 1.0f/sqrtf((f32)EMBED_DIM);
    for(u32 v=0;v<vocab_size;v++)
        for(u32 d=0;d<EMBED_DIM;d++)
            m->embed[v][d]=rng_f32()*se;

    /* encoding posicional */
    for(u32 pos=0;pos<SEQ_LEN;pos++)
        for(u32 i=0;i<EMBED_DIM;i+=2){
            f32 freq=1.0f/powf(10000.0f,(f32)i/(f32)EMBED_DIM);
            m->pos_enc[pos][i]  =sinf((f32)pos*freq);
            m->pos_enc[pos][i+1]=cosf((f32)pos*freq);
        }

    /* pesos transformer — Xavier */
    f32 sa = 1.0f/sqrtf((f32)EMBED_DIM);
    f32 sf = 1.0f/sqrtf((f32)HIDDEN_DIM);
    f32 *W_list[]={
        &m->Wq0[0][0],&m->Wk0[0][0],&m->Wv0[0][0],&m->Wo0[0][0],
        &m->Wq1[0][0],&m->Wk1[0][0],&m->Wv1[0][0],&m->Wo1[0][0],NULL
    };
    for(u32 w=0;W_list[w];w++)
        for(u32 i=0;i<EMBED_DIM*EMBED_DIM;i++)
            W_list[w][i]=rng_f32()*sa;

    for(u32 i=0;i<EMBED_DIM*HIDDEN_DIM;i++){
        m->W1_0[0][i]=rng_f32()*sa;
        m->W1_1[0][i]=rng_f32()*sa;
    }
    for(u32 i=0;i<HIDDEN_DIM*EMBED_DIM;i++){
        m->W2_0[0][i]=rng_f32()*sf;
        m->W2_1[0][i]=rng_f32()*sf;
    }
    /* bias e layer norm */
    memset(m->b1_0,0,sizeof(m->b1_0)); memset(m->b2_0,0,sizeof(m->b2_0));
    memset(m->b1_1,0,sizeof(m->b1_1)); memset(m->b2_1,0,sizeof(m->b2_1));
    for(u32 i=0;i<EMBED_DIM;i++){
        m->ln1a_0[i]=m->ln2a_0[i]=m->ln1a_1[i]=m->ln2a_1[i]=1.0f;
        m->ln1b_0[i]=m->ln2b_0[i]=m->ln1b_1[i]=m->ln2b_1[i]=0.0f;
    }
    /* head */
    f32 sh=1.0f/sqrtf((f32)EMBED_DIM);
    for(u32 i=0;i<EMBED_DIM;i++)
        for(u32 j=0;j<vocab_size;j++)
            m->head_W[i][j]=rng_f32()*sh;
    memset(m->head_b,0,vocab_size*sizeof(f32));
}

static Model *model_new(u32 vocab_size) {
    Model *m = ALLOC64(Model,1);
    if(!m) return NULL;
    model_init_weights(m, vocab_size);
    return m;
}

/* ---- forward pass de uma camada transformer ---- */
static void transformer_layer_fwd(
        const f32 *Wq, const f32 *Wk, const f32 *Wv, const f32 *Wo,
        const f32 *W1, const f32 *W2,
        const f32 *b1, const f32 *b2,
        const f32 *ln1a, const f32 *ln1b,
        const f32 *ln2a, const f32 *ln2b,
        const f32 * restrict x,  /* [seq][embed] */
        f32 * restrict out,       /* [seq][embed] */
        u32 seq_len) {

    f32 scale = 1.0f/sqrtf((f32)EMBED_DIM);

    /* LN1 */
    for(u32 s=0;s<seq_len;s++)
        layer_norm(g_tmp[s], x+s*EMBED_DIM, ln1a, ln1b, EMBED_DIM);

    /* Q, K, V */
    matmul_seq(&g_Q[0][0], &g_tmp[0][0], Wq, seq_len, EMBED_DIM, EMBED_DIM);
    matmul_seq(&g_K[0][0], &g_tmp[0][0], Wk, seq_len, EMBED_DIM, EMBED_DIM);
    matmul_seq(&g_V[0][0], &g_tmp[0][0], Wv, seq_len, EMBED_DIM, EMBED_DIM);

    /* scores causais + softmax */
    for(u32 i=0;i<seq_len;i++){
        for(u32 j=0;j<seq_len;j++)
            g_scores[i][j] = (j>i) ? -1e9f :
                dot_f32(g_Q[i], g_K[j], EMBED_DIM)*scale;
        softmax_f32(g_scores[i], seq_len);
    }

    /* attn_out = scores * V */
    for(u32 i=0;i<seq_len;i++){
        vec_zero(g_tmp[i], EMBED_DIM);
        for(u32 j=0;j<seq_len;j++){
            f32 a=g_scores[i][j]; const f32 *vj=g_V[j]; u32 d=0;
            for(;d+3<EMBED_DIM;d+=4){
                g_tmp[i][d]+=a*vj[d]; g_tmp[i][d+1]+=a*vj[d+1];
                g_tmp[i][d+2]+=a*vj[d+2]; g_tmp[i][d+3]+=a*vj[d+3];
            }
            for(;d<EMBED_DIM;d++) g_tmp[i][d]+=a*vj[d];
        }
    }

    /* projeção Wo + residual */
    matmul_seq(&g_tmp2[0][0], &g_tmp[0][0], Wo, seq_len, EMBED_DIM, EMBED_DIM);
    for(u32 s=0;s<seq_len;s++){
        const f32 *xi=x+s*EMBED_DIM;
        for(u32 d=0;d<EMBED_DIM;d++) g_tmp2[s][d]+=xi[d];
    }

    /* LN2 -> FFN -> residual */
    for(u32 s=0;s<seq_len;s++)
        layer_norm(g_tmp[s], g_tmp2[s], ln2a, ln2b, EMBED_DIM);

    /* FFN W1 -> relu -> W2 */
    matmul_seq(&g_hidden[0][0], &g_tmp[0][0], W1, seq_len, EMBED_DIM, HIDDEN_DIM);
    for(u32 s=0;s<seq_len;s++)
        for(u32 j=0;j<HIDDEN_DIM;j++) g_hidden[s][j]=reluf(g_hidden[s][j]+b1[j]);
    matmul_seq(out, &g_hidden[0][0], W2, seq_len, HIDDEN_DIM, EMBED_DIM);
    for(u32 s=0;s<seq_len;s++){
        const f32 *r=g_tmp2[s];
        for(u32 d=0;d<EMBED_DIM;d++) out[s*EMBED_DIM+d]+=r[d]+b2[d];
    }
}

/* forward completo — preenche g_x[0..seq_len][EMBED_DIM] */
static void model_forward(Model *m, const u32 *ids, u32 seq_len) {
    /* embed + pos */
    for(u32 p=0;p<seq_len;p++){
        u32 tid=ids[p]; if(tid>=m->vocab_size)tid=1;
        for(u32 d=0;d<EMBED_DIM;d++)
            g_x[p][d]=m->embed[tid][d]+m->pos_enc[p][d];
    }
    /* camada 0 */
    transformer_layer_fwd(
        &m->Wq0[0][0],&m->Wk0[0][0],&m->Wv0[0][0],&m->Wo0[0][0],
        &m->W1_0[0][0],&m->W2_0[0][0],m->b1_0,m->b2_0,
        m->ln1a_0,m->ln1b_0,m->ln2a_0,m->ln2b_0,
        &g_x[0][0], &g_tmp2[0][0], seq_len);
    /* camada 1 */
    transformer_layer_fwd(
        &m->Wq1[0][0],&m->Wk1[0][0],&m->Wv1[0][0],&m->Wo1[0][0],
        &m->W1_1[0][0],&m->W2_1[0][0],m->b1_1,m->b2_1,
        m->ln1a_1,m->ln1b_1,m->ln2a_1,m->ln2b_1,
        &g_tmp2[0][0], &g_x[0][0], seq_len);
}

/* ============================================================
   BIGRAM RÁPIDO — treino principal (convergência garantida)
   ============================================================ */
static f32 bigram_step(Model *m, u32 ctx_id, u32 tgt_id, f32 lr) {
    if(ctx_id>=m->vocab_size||tgt_id>=m->vocab_size) return 0.0f;
    f32 *emb = m->embed[ctx_id];

    for(u32 j=0;j<m->vocab_size;j++){
        f32 s=m->head_b[j];
        for(u32 i=0;i<EMBED_DIM;i++) s+=m->head_W[i][j]*emb[i];
        g_logits[j]=s;
    }
    /* softmax */
    f32 mx=g_logits[0];
    for(u32 j=1;j<m->vocab_size;j++) if(g_logits[j]>mx) mx=g_logits[j];
    f32 sum=0;
    for(u32 j=0;j<m->vocab_size;j++){g_logits[j]=fast_expf(g_logits[j]-mx);sum+=g_logits[j];}
    f32 inv=sum>1e-9f?1.0f/sum:0.0f;
    for(u32 j=0;j<m->vocab_size;j++) g_logits[j]*=inv;

    f32 loss=-logf(g_logits[tgt_id]+1e-9f);

    /* backward */
    static f32 ALIGNED16 dL[VOCAB_MAX];
    for(u32 j=0;j<m->vocab_size;j++)
        dL[j]=g_logits[j]-(j==tgt_id?1.0f:0.0f);

    for(u32 i=0;i<EMBED_DIM;i++){
        f32 ei=emb[i];
        for(u32 j=0;j<m->vocab_size;j++) m->head_W[i][j]-=lr*dL[j]*ei;
    }
    for(u32 j=0;j<m->vocab_size;j++) m->head_b[j]-=lr*dL[j];
    for(u32 i=0;i<EMBED_DIM;i++){
        f32 g2=0;
        for(u32 j=0;j<m->vocab_size;j++) g2+=m->head_W[i][j]*dL[j];
        emb[i]-=lr*g2;
    }
    m->step++;
    return loss;
}

/* ============================================================
   MÉTRICAS — perplexidade, entropia, coerência
   ============================================================ */
typedef struct {
    f32 loss;
    f32 perplexity;
    f32 entropy;       /* entropia do vocabulário de predições */
    f32 top1_acc;      /* acurácia top-1 */
    u32 n_samples;
    f64 elapsed_ms;
    f32 tokens_per_sec;
} Metrics;

static Metrics compute_metrics(Model *m,
                                 const u32 *ids, u32 n_ids) {
    Metrics met={0};
    if(n_ids<2) return met;
    struct timespec t0,t1;
    clock_gettime(CLOCK_MONOTONIC,&t0);

    f32 total_loss=0; u32 correct=0;
    for(u32 i=0;i+1<n_ids&&i<512;i++){
        u32 ctx=ids[i], tgt=ids[i+1];
        if(ctx>=m->vocab_size||tgt>=m->vocab_size) continue;

        f32 *emb=m->embed[ctx];
        for(u32 j=0;j<m->vocab_size;j++){
            f32 s=m->head_b[j];
            for(u32 d=0;d<EMBED_DIM;d++) s+=m->head_W[d][j]*emb[d];
            g_logits[j]=s;
        }
        f32 mx2=g_logits[0];
        for(u32 j=1;j<m->vocab_size;j++) if(g_logits[j]>mx2) mx2=g_logits[j];
        f32 sum2=0;
        for(u32 j=0;j<m->vocab_size;j++){g_logits[j]=fast_expf(g_logits[j]-mx2);sum2+=g_logits[j];}
        f32 inv2=sum2>1e-9f?1.0f/sum2:0;
        for(u32 j=0;j<m->vocab_size;j++) g_logits[j]*=inv2;

        total_loss+=-logf(g_logits[tgt]+1e-9f);

        u32 top=0;
        for(u32 j=1;j<m->vocab_size;j++) if(g_logits[j]>g_logits[top]) top=j;
        if(top==tgt) correct++;

        /* entropia desta distribuição */
        for(u32 j=0;j<m->vocab_size;j++)
            if(g_logits[j]>1e-9f) met.entropy+=-g_logits[j]*logf(g_logits[j]);
        met.n_samples++;
    }

    clock_gettime(CLOCK_MONOTONIC,&t1);
    met.elapsed_ms=(t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)*1e-6;

    if(met.n_samples>0){
        met.loss=total_loss/(f32)met.n_samples;
        met.perplexity=expf(met.loss);
        met.entropy/=(f32)met.n_samples;
        met.top1_acc=(f32)correct/(f32)met.n_samples;
        met.tokens_per_sec=(f32)met.n_samples/(f32)(met.elapsed_ms*0.001);
    }
    return met;
}

static void print_metrics(const Metrics *met, const char *label) {
    printf("\n  " BOLD YEL "MÉTRICAS: %s\n" RST, label);
    printf("  %-22s " GRN "%.4f\n" RST, "Loss (NLL):",    met->loss);
    printf("  %-22s " GRN "%.2f\n" RST,  "Perplexidade:", met->perplexity);
    printf("  %-22s " GRN "%.4f bits\n" RST,"Entropia:", met->entropy/logf(2.0f));
    printf("  %-22s " GRN "%.2f%%\n" RST, "Acurácia top-1:",met->top1_acc*100.0f);
    printf("  %-22s " GRN "%u\n" RST,     "Amostras:",    met->n_samples);
    printf("  %-22s " GRN "%.1f ms\n" RST,"Tempo:",       met->elapsed_ms);
    printf("  %-22s " GRN "%.0f tok/s\n" RST,"Throughput:",met->tokens_per_sec);
}

/* ============================================================
   BENCHMARK COM MEDIANAS
   ============================================================ */
static int f32_cmp(const void *a, const void *b){
    f32 x=*(const f32*)a, y=*(const f32*)b;
    return x<y?-1:x>y?1:0;
}

typedef struct {
    f32 median_ms;
    f32 mean_ms;
    f32 p95_ms;
    f32 p5_ms;
    f32 min_ms;
    f32 max_ms;
    f32 tok_per_sec;
    u32 n_runs;
} BenchResult;

static BenchResult benchmark_forward(Model *m,
                                      const u32 *ids, u32 n_ids,
                                      u32 bench_steps) {
    BenchResult br={0}; br.n_runs=bench_steps;
    static f32 times[BENCH_RUNS*2];
    u32 nruns=bench_steps<BENCH_RUNS*2?bench_steps:BENCH_RUNS*2;
    struct timespec t0,t1;

    for(u32 r=0;r<nruns;r++){
        u32 idx=r%(n_ids>1?n_ids-1:1);
        clock_gettime(CLOCK_MONOTONIC,&t0);
        bigram_step(m, ids[idx], ids[idx+1], 0.0f); /* lr=0 sem update */
        clock_gettime(CLOCK_MONOTONIC,&t1);
        times[r]=(f32)((t1.tv_sec-t0.tv_sec)*1e6+(t1.tv_nsec-t0.tv_nsec)*1e-3);
    }
    qsort(times, nruns, sizeof(f32), f32_cmp);
    br.min_ms=times[0]/1000.0f;
    br.max_ms=times[nruns-1]/1000.0f;
    br.median_ms=times[nruns/2]/1000.0f;
    br.p5_ms=times[(u32)(nruns*0.05f)]/1000.0f;
    br.p95_ms=times[(u32)(nruns*0.95f)]/1000.0f;
    f32 sum3=0;
    for(u32 r=0;r<nruns;r++) sum3+=times[r];
    br.mean_ms=(sum3/nruns)/1000.0f;
    br.tok_per_sec=1000.0f/br.median_ms;
    return br;
}

static void print_benchmark(const BenchResult *br) {
    printf("\n  " BOLD MAG "BENCHMARK ARM32 (n=%u)\n" RST, br->n_runs);
    printf("  %-18s " YEL "%8.3f ms\n" RST, "Mediana:",   br->median_ms);
    printf("  %-18s " YEL "%8.3f ms\n" RST, "Média:",     br->mean_ms);
    printf("  %-18s " YEL "%8.3f ms\n" RST, "P5:",        br->p5_ms);
    printf("  %-18s " YEL "%8.3f ms\n" RST, "P95:",       br->p95_ms);
    printf("  %-18s " YEL "%8.3f ms\n" RST, "Mín:",       br->min_ms);
    printf("  %-18s " YEL "%8.3f ms\n" RST, "Máx:",       br->max_ms);
    printf("  %-18s " GRN "%8.0f tok/s\n" RST,"Throughput:", br->tok_per_sec);

    /* barra visual */
    printf("\n  Latência (escala relativa):\n");
    f32 scale = 40.0f / (br->p95_ms > 0.001f ? br->p95_ms : 1.0f);
    const char *labels[]={"P5","Med","P95"};
    f32 vals[]={br->p5_ms, br->median_ms, br->p95_ms};
    for(u32 i=0;i<3;i++){
        u32 bar=(u32)(vals[i]*scale);
        if(bar>40)bar=40;
        printf("  " YEL "%-4s" RST " [", labels[i]);
        for(u32 j=0;j<bar;j++) printf(GRN "█" RST);
        for(u32 j=bar;j<40;j++) printf(DIM "·" RST);
        printf("] %.3f ms\n", vals[i]);
    }
}

/* ============================================================
   SAVE / LOAD
   ============================================================ */
typedef struct {
    u32 magic;
    u32 vocab_size;
    u32 step;
    u32 embed_dim;
    f32 best_loss;
    u32 _pad[3];
} ModelHeader;

static i32 model_save(const Model *m, const Vocab *v,
                       const char *path) {
    FILE *fp=fopen(path,"wb"); if(!fp)return -1;
    ModelHeader hdr={MODEL_MAGIC,m->vocab_size,m->step,
                     EMBED_DIM,m->best_loss,{0,0,0}};
    fwrite(&hdr,sizeof(hdr),1,fp);
    fwrite(m->embed,sizeof(f32),m->vocab_size*EMBED_DIM,fp);
    fwrite(m->head_W,sizeof(f32),EMBED_DIM*m->vocab_size,fp);
    fwrite(m->head_b,sizeof(f32),m->vocab_size,fp);
    if(v){
        fwrite(&v->size,sizeof(u32),1,fp);
        fwrite(v->entries,sizeof(VocabEntry),v->cap,fp);
    }
    fclose(fp);
    printf("  " GRN "[save]" RST " %s  vocab=%u step=%u loss=%.4f\n",
           path,m->vocab_size,m->step,m->best_loss);
    return 0;
}

static i32 model_load(Model *m, Vocab *v, const char *path) {
    FILE *fp=fopen(path,"rb"); if(!fp)return -1;
    ModelHeader hdr;
    if(fread(&hdr,sizeof(hdr),1,fp)!=1){fclose(fp);return -2;}
    if(hdr.magic!=MODEL_MAGIC||hdr.embed_dim!=EMBED_DIM){fclose(fp);return -3;}
    m->vocab_size=hdr.vocab_size;
    m->step=hdr.step;
    m->best_loss=hdr.best_loss;
    fread(m->embed,sizeof(f32),m->vocab_size*EMBED_DIM,fp);
    fread(m->head_W,sizeof(f32),EMBED_DIM*m->vocab_size,fp);
    fread(m->head_b,sizeof(f32),m->vocab_size,fp);
    if(v){
        fread(&v->size,sizeof(u32),1,fp);
        fread(v->entries,sizeof(VocabEntry),v->cap,fp);
    }
    fclose(fp);
    printf("  " GRN "[load]" RST " %s  vocab=%u step=%u loss=%.4f\n",
           path,m->vocab_size,m->step,m->best_loss);
    return 0;
}

/* ============================================================
   GERAÇÃO — com temperatura e sampling
   ============================================================ */
static void model_generate(Model *m, Vocab *v,
                             u32 start_id, u32 n_gen, f32 temp) {
    u32 cur=start_id;
    u32 rng2=(u32)time(NULL);
    printf("  " BOLD GRN "» " RST);
    for(u32 i=0;i<n_gen;i++){
        f32 *emb=m->embed[cur<m->vocab_size?cur:1];
        for(u32 j=0;j<m->vocab_size;j++){
            f32 s=m->head_b[j];
            for(u32 d=0;d<EMBED_DIM;d++) s+=m->head_W[d][j]*emb[d];
            g_logits[j]=s/(temp>0.01f?temp:0.01f);
        }
        f32 mx2=g_logits[0];
        for(u32 j=1;j<m->vocab_size;j++) if(g_logits[j]>mx2) mx2=g_logits[j];
        f32 sum2=0;
        for(u32 j=0;j<m->vocab_size;j++){g_logits[j]=fast_expf(g_logits[j]-mx2);sum2+=g_logits[j];}
        rng2=rng2*1664525u+1013904223u;
        f32 r2=((f32)(rng2>>1))/(f32)0x7FFFFFFFu*sum2;
        u32 sampled=0; f32 acc2=0;
        for(u32 j=0;j<m->vocab_size;j++){
            acc2+=g_logits[j];
            if(acc2>=r2){sampled=j;break;}
        }
        const char *word=vocab_str(v,sampled);
        /* colore tokens imagem de forma diferente */
        if(word[0]=='i'&&word[1]=='m'&&word[2]=='g')
            printf(BLU "%s " RST, word);
        else
            printf(WHT "%s " RST, word);
        cur=sampled;
        if(cur==v->id_eos) break;
    }
    printf("\n");
}

/* ============================================================
   TREINO COM PROGRESSO
   ============================================================ */
static void train_loop(Model *m, const u32 *ids, u32 n_ids,
                        u32 n_epochs, f32 lr, const char *save_path) {
    if(n_ids<2){printf("  " RED "[erro]" RST " tokens insuficientes\n");return;}
    printf(BOLD "\n  [train] tokens=%u epochs=%u lr=%.5f vocab=%u\n" RST,
           n_ids,n_epochs,lr,m->vocab_size);

    struct timespec t0,t1;
    clock_gettime(CLOCK_MONOTONIC,&t0);
    f32 prev_loss=1e9f;

    for(u32 ep=0;ep<n_epochs;ep++){
        f32 total=0; u32 cnt=0;
        for(u32 i=0;i+1<n_ids;i++){
            total+=bigram_step(m,ids[i],ids[i+1],lr);
            cnt++;
        }
        f32 loss=cnt?total/(f32)cnt:0.0f;
        if(loss<m->best_loss) m->best_loss=loss;

        if(ep%10==0||ep==n_epochs-1){
            clock_gettime(CLOCK_MONOTONIC,&t1);
            f64 el=(t1.tv_sec-t0.tv_sec)+(t1.tv_nsec-t0.tv_nsec)*1e-9;
            f32 delta=prev_loss-loss;
            const char *trend = delta>0.001f?GRN"↓":delta<-0.001f?RED"↑":YEL"→";
            printf("  ep=%3u  loss=" YEL "%.4f" RST " %s" RST
                   "  best=%.4f  %.1fs\n",
                   ep, loss, trend, m->best_loss, el);
            prev_loss=loss;
        }
    }
    if(save_path) model_save(m,NULL,save_path);
}

/* ============================================================
   DASHBOARD DE PARÂMETROS DO SISTEMA
   ============================================================ */
static void print_params(const Model *m) {
    u32 embed_p  = m->vocab_size * EMBED_DIM;
    u32 attn_p   = N_LAYERS * 4 * EMBED_DIM * EMBED_DIM;
    u32 ffn_p    = N_LAYERS * (EMBED_DIM*HIDDEN_DIM + HIDDEN_DIM*EMBED_DIM);
    u32 head_p   = EMBED_DIM * m->vocab_size;
    u32 total_p  = embed_p + attn_p + ffn_p + head_p;
    f32 mem_mb   = (f32)(total_p*4)/(1024.0f*1024.0f);

    printf(BOLD "\n  PARÂMETROS DO MODELO\n" RST);
    printf("  %-24s " CYN "%u\n" RST, "vocab_size:",   m->vocab_size);
    printf("  %-24s " CYN "%d\n" RST, "embed_dim:",    EMBED_DIM);
    printf("  %-24s " CYN "%d\n" RST, "hidden_dim:",   HIDDEN_DIM);
    printf("  %-24s " CYN "%d\n" RST, "n_layers:",     N_LAYERS);
    printf("  %-24s " CYN "%d\n" RST, "n_heads:",      N_HEADS);
    printf("  %-24s " CYN "%d\n" RST, "head_dim:",     HEAD_DIM);
    printf("  %-24s " CYN "%d\n" RST, "seq_len:",      SEQ_LEN);
    printf("  %-24s " CYN "%d MB\n" RST,"arena_total:", ARENA_MB);
    printf("  " DIM "─────────────────────────────────\n" RST);
    printf("  %-24s " YEL "%u\n" RST, "params embedding:",  embed_p);
    printf("  %-24s " YEL "%u\n" RST, "params atenção:",    attn_p);
    printf("  %-24s " YEL "%u\n" RST, "params FFN:",        ffn_p);
    printf("  %-24s " YEL "%u\n" RST, "params head:",       head_p);
    printf("  %-24s " BOLD GRN "%u (%.2f MB)\n" RST, "TOTAL params:", total_p, mem_mb);
    printf("  %-24s " GRN "%u\n" RST, "steps treinados:",   m->step);
    printf("  %-24s " GRN "%.4f\n" RST,"melhor loss:",      m->best_loss);
}

/* ============================================================
   FETCH HTTP — wget/curl
   ============================================================ */
static char g_text_buf[TEXT_BUF_SIZE];

static i32 fetch_url(const char *url, char *buf,
                      u32 buf_size, u32 *out_len) {
    char cmd[512];
    snprintf(cmd,sizeof(cmd),
             "wget -q --timeout=15 -O - \"%s\" 2>/dev/null",url);
    FILE *fp=popen(cmd,"r");
    if(!fp){
        snprintf(cmd,sizeof(cmd),
                 "curl -s --max-time 15 \"%s\" 2>/dev/null",url);
        fp=popen(cmd,"r");
        if(!fp) return -1;
    }
    u32 total=0; size_t r2;
    while(total<buf_size-1&&(r2=fread(buf+total,1,4096,fp))>0)
        total+=(u32)r2;
    buf[total]=0; pclose(fp);
    if(out_len)*out_len=total;
    return total>0?0:-1;
}

static i32 load_file(const char *path, char *buf,
                      u32 buf_size, u32 *out_len) {
    FILE *fp=fopen(path,"rb"); if(!fp)return -1;
    u32 total=0; size_t r2;
    while(total<buf_size-1&&(r2=fread(buf+total,1,65536,fp))>0)
        total+=(u32)r2;
    buf[total]=0; fclose(fp);
    if(out_len)*out_len=total;
    return 0;
}

/* ============================================================
   AJUDA
   ============================================================ */
static void print_help(void) {
    print_separator("COMANDOS");
    const char *cmds[][3]={
        {"train <arquivo>","[N] [lr]","treina com texto, JSON ou PPM"},
        {"fetch",         "",        "baixa escrituras (wget/curl)"},
        {"gen <palavra>", "[N] [t]", "gera N tokens (temp=t)"},
        {"eval",         "",        "calcula métricas no corpus atual"},
        {"bench",        "[N]",     "benchmark com medianas (N runs)"},
        {"params",       "",        "exibe parâmetros do modelo"},
        {"vocab",        "[N]",     "top-N tokens por frequência"},
        {"save <arq>",   "",        "salva pesos"},
        {"load <arq>",   "",        "carrega pesos"},
        {"logo",         "",        "exibe logo"},
        {"help",         "",        "esta ajuda"},
        {"quit",         "",        "sair"},
        {NULL,NULL,NULL}
    };
    for(u32 i=0;cmds[i][0];i++)
        printf("  " YEL "%-26s" CYN "%-12s" RST " %s\n",
               cmds[i][0],cmds[i][1],cmds[i][2]);

    printf("\n  " DIM "Compilar (Termux ARM32):\n" RST);
    printf("  " DIM CYN "  clang -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 \\\n" RST);
    printf("  " DIM CYN "        -mfloat-abi=softfp -std=c11 \\\n" RST);
    printf("  " DIM CYN "        geolm.c -lm -o geolm && ./geolm\n\n" RST);
}

/* ============================================================
   TEXTO SEMENTE EMBUTIDO (sempre disponível offline)
   ============================================================ */
static const char SEED_TEXT[] =
    "no principio era o verbo e o verbo estava com deus "
    "o verbo era deus ele estava no principio com deus "
    "pela fe entendemos que os mundos foram criados pela palavra de deus "
    "o senhor e o meu pastor nada me faltara ainda que eu ande "
    "pelo vale da sombra da morte nao temerei mal algum "
    "pois tu estas comigo a tua vara e o teu cajado me consolam "
    "a sabedoria clama nas ruas levanta a sua voz nas pracas "
    "o temor do senhor e o principio da sabedoria "
    "conhecereis a verdade e a verdade vos libertara "
    "eu sou o caminho a verdade e a vida "
    "no principio criou deus os ceus e a terra "
    "e a terra era sem forma e vazia "
    "e o espirito de deus pairava sobre a face das aguas "
    "e deus disse haja luz e houve luz "
    "o dao que pode ser expresso nao e o dao eterno "
    "o nome que pode ser nomeado nao e o nome eterno "
    "sem nome e o principio do ceu e da terra "
    "em quietude encontramos o espelho do universo "
    "a agua e o mais suave de todos os elementos "
    "e todavia perfura a rocha mais dura "
    "o sábio age sem agir e ensina sem palavras "
    "entropia e sintropia coerencia incoerencia "
    "invariante geometrica fractal toroidal "
    "hebraico aramaico grego latim arabico "
    "matematica prova teorema axioma lema "
    "frequencia hertz ondas campo vetorial atrator "
    "linguagem estrutura estado transicao significado";

/* ============================================================
   MAIN + REPL
   ============================================================ */
static u32 g_ids[IDS_MAX];
static u32 g_n_ids = 0;
static char g_extracted[TEXT_BUF_SIZE];

int main(int argc, char **argv) {
    arena_init(&g_arena);

    Vocab *vocab = vocab_new();
    if(!vocab){fprintf(stderr,"[FAIL] vocab OOM\n");return 1;}

    /* carrega semente */
    g_n_ids = tokenize(vocab,(char*)SEED_TEXT,
                        (u32)strlen(SEED_TEXT),g_ids,IDS_MAX);

    Model *model = model_new(vocab->size);
    if(!model){fprintf(stderr,"[FAIL] model OOM\n");return 1;}

    /* modo linha de comando */
    if(argc>=2){
        if(!strcmp(argv[1],"help")){
            print_logo();print_help();return 0;
        }
        if(!strcmp(argv[1],"version")){
            printf("GeoLM ARM32 v2.0 — build %s\n",__DATE__);return 0;
        }
        /* treinamento direto */
        if(!strcmp(argv[1],"train")&&argc>=3){
            u32 len=0;
            load_file(argv[2],g_text_buf,TEXT_BUF_SIZE,&len);
            if(len>0){
                u32 toks=0;
                if(g_text_buf[0]=='{'||g_text_buf[0]=='['){
                    u32 elen=json_extract_text(g_text_buf,len,
                                               g_extracted,TEXT_BUF_SIZE);
                    toks=tokenize(vocab,g_extracted,elen,
                                  g_ids,IDS_MAX);
                } else if(len>6&&(u8)g_text_buf[0]=='P'&&
                          ((u8)g_text_buf[1]=='6'||(u8)g_text_buf[1]=='3')){
                    toks=ppm_to_tokens(vocab,(u8*)g_text_buf,len,
                                       g_ids,IDS_MAX);
                    printf("  " CYN "[ppm]" RST " %u tokens visuais\n",toks);
                } else {
                    toks=tokenize(vocab,g_text_buf,len,g_ids,IDS_MAX);
                }
                g_n_ids=toks;
                model=model_new(vocab->size);
            }
            u32 ep = argc>=4 ? (u32)atoi(argv[3]) : 50;
            f32 lr = argc>=5 ? (f32)atof(argv[4]) : 0.05f;
            train_loop(model,g_ids,g_n_ids,ep,lr,"geolm.bin");
            Metrics met=compute_metrics(model,g_ids,g_n_ids);
            print_metrics(&met,"pós-treino");
            return 0;
        }
    }

    /* treino inicial com semente */
    print_logo();
    printf("  " DIM "Inicializando com texto semente (%u tokens)...\n\n" RST,
           g_n_ids);
    train_loop(model,g_ids,g_n_ids,30,0.05f,NULL);

    print_help();

    /* REPL */
    char line[256];
    for(;;){
        printf("\n" BOLD CYN "  geolm" YEL "> " RST);
        fflush(stdout);
        if(!fgets(line,sizeof(line),stdin)) break;
        line[strcspn(line,"\n")]=0;
        if(!line[0]) continue;

        char cmd[64],a1[128],a2[32],a3[32];
        cmd[0]=a1[0]=a2[0]=a3[0]=0;
        sscanf(line,"%63s %127s %31s %31s",cmd,a1,a2,a3);

        if(!strcmp(cmd,"quit")||!strcmp(cmd,"exit")||!strcmp(cmd,"q")){
            printf(GRN "  Até logo. Ω\n" RST); break;
        }
        else if(!strcmp(cmd,"logo"))  print_logo();
        else if(!strcmp(cmd,"help"))  print_help();
        else if(!strcmp(cmd,"params"))print_params(model);

        else if(!strcmp(cmd,"vocab")){
            u32 n = a1[0]?(u32)atoi(a1):20;
            printf("  vocab size=%u cap=%u\n",vocab->size,vocab->cap);
            vocab_top_n(vocab,n);
        }

        else if(!strcmp(cmd,"gen")){
            u32 start=vocab->id_bos, n=24;
            f32 temp=0.8f;
            if(a1[0]) start=vocab_lookup(vocab,a1);
            if(a2[0]) n=(u32)atoi(a2);
            if(a3[0]) temp=(f32)atof(a3);
            if(start==vocab->id_unk){
                printf("  " YEL "palavra não encontrada, usando <bos>\n" RST);
                start=vocab->id_bos;
            }
            model_generate(model,vocab,start,n,temp);
        }

        else if(!strcmp(cmd,"eval")){
            Metrics met=compute_metrics(model,g_ids,g_n_ids);
            print_metrics(&met,"corpus atual");
            arena_print_stats(&g_arena,"memória arena");
        }

        else if(!strcmp(cmd,"bench")){
            u32 n=a1[0]?(u32)atoi(a1):BENCH_RUNS;
            printf("  Rodando %u iterações...\n",n);
            BenchResult br=benchmark_forward(model,g_ids,g_n_ids,n);
            print_benchmark(&br);
        }

        else if(!strcmp(cmd,"train")){
            u32 ep = a1[0]?(u32)atoi(a1):20;
            f32 lr = a2[0]?(f32)atof(a2):0.05f;
            train_loop(model,g_ids,g_n_ids,ep,lr,NULL);
        }

        else if(!strcmp(cmd,"load_text")&&a1[0]){
            u32 len=0;
            i32 ok=load_file(a1,g_text_buf,TEXT_BUF_SIZE,&len);
            if(ok!=0){printf("  " RED "não encontrado: %s\n" RST,a1);continue;}
            u32 toks=0;
            /* detecta tipo */
            if(len>4&&g_text_buf[0]=='{'||g_text_buf[0]=='['){
                printf("  " CYN "[json] extraindo...\n" RST);
                u32 el=json_extract_text(g_text_buf,len,g_extracted,TEXT_BUF_SIZE);
                toks=tokenize(vocab,g_extracted,el,g_ids,IDS_MAX);
            } else if(len>6&&(u8)g_text_buf[0]=='P'&&
                       ((u8)g_text_buf[1]=='6'||(u8)g_text_buf[1]=='3')){
                printf("  " CYN "[ppm] convertendo pixels...\n" RST);
                toks=ppm_to_tokens(vocab,(u8*)g_text_buf,len,g_ids,IDS_MAX);
            } else {
                toks=tokenize(vocab,g_text_buf,len,g_ids,IDS_MAX);
            }
            g_n_ids=toks;
            model=model_new(vocab->size);
            printf("  " GRN "carregado: %u tokens, vocab=%u\n" RST,
                   g_n_ids,vocab->size);
        }

        else if(!strcmp(cmd,"fetch")){
            const char *urls[]={
                "https://www.gutenberg.org/cache/epub/10/pg10.txt",
                "https://www.gutenberg.org/cache/epub/216/pg216.txt",
                NULL
            };
            for(u32 u=0;urls[u]&&g_n_ids<55000;u++){
                printf("  " CYN "fetch: %s\n" RST,urls[u]);
                u32 len2=0;
                if(fetch_url(urls[u],g_text_buf,TEXT_BUF_SIZE,&len2)==0){
                    u32 toks=tokenize(vocab,g_text_buf,len2,
                                      g_ids+g_n_ids,IDS_MAX-g_n_ids);
                    g_n_ids+=toks;
                    printf("  " GRN "+%u tokens (total=%u vocab=%u)\n" RST,
                           toks,g_n_ids,vocab->size);
                } else {
                    printf("  " YEL "[aviso] wget/curl indisponível\n" RST);
                }
            }
            if(g_n_ids>100){
                model=model_new(vocab->size);
                train_loop(model,g_ids,g_n_ids,20,0.05f,NULL);
            }
        }

        else if(!strcmp(cmd,"save")&&a1[0])
            model_save(model,vocab,a1);

        else if(!strcmp(cmd,"load")&&a1[0]){
            Model *m2=ALLOC64(Model,1);
            if(m2&&model_load(m2,vocab,a1)==0) model=m2;
            else printf("  " RED "[erro] falha ao carregar\n" RST);
        }

        else if(!strcmp(cmd,"mem"))
            arena_print_stats(&g_arena,"arena global");

        else if(!strcmp(cmd,"reset")){
            arena_init(&g_arena);
            vocab=vocab_new();
            g_n_ids=tokenize(vocab,(char*)SEED_TEXT,
                             (u32)strlen(SEED_TEXT),g_ids,IDS_MAX);
            model=model_new(vocab->size);
            printf("  " YEL "sistema reiniciado\n" RST);
        }

        else
            printf("  " DIM "desconhecido: %s  (digite help)\n" RST, cmd);
    }
    return 0;
}
