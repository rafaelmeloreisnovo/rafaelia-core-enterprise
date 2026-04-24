#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <math.h>

// =============================
// TIMER REAL (ARM64)
// =============================
static inline uint64_t rdcycle() {
    uint64_t v;
#if defined(__aarch64__)
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
#else
    v = 0;
#endif
    return v;
}

// =============================
// WRITE DIRETO
// =============================
static void out(const char *s) {
    int n=0; while(s[n]) n++;
    syscall(SYS_write,1,s,n);
}

// =============================
// HEX PRINT
// =============================
static void hex64(uint64_t v){
    char h[]="0123456789abcdef";
    char o[16];
    for(int i=15;i>=0;i--){ o[i]=h[v&0xF]; v>>=4; }
    syscall(SYS_write,1,o,16);
}

// =============================
// CRC SOFTWARE (universal)
// =============================
static uint32_t crc32(uint8_t *p, int n){
    uint32_t c=0;
    for(int i=0;i<n;i++){
        c^=p[i];
        for(int j=0;j<8;j++)
            c=(c>>1)^(0xEDB88320 & -(c&1));
    }
    return c;
}

// =============================
// MIX (variante)
// =============================
static void mix(uint8_t *p){
    for(int i=0;i<4096;i+=64){
        for(int j=0;j<64;j++){
            uint8_t a=p[i+((j+7)&63)];
            uint8_t b=p[i+((j+11)&63)];
            p[i+j]^=a^b;
        }
    }
}

// =============================
// LOG2 APROX (sem libc)
// =============================
static double fast_log2(double x){
    int e;
    double m = frexp(x, &e);
    return (double)e + (m - 0.70710678); // aproximação
}

// =============================
// MAIN
// =============================
int main(){

    static uint8_t buf[4096];

    // init determinístico
    uint64_t s=0x9E3779B97F4A7C15ULL;
    for(int i=0;i<4096;i+=8){
        *(uint64_t*)(buf+i)=s;
        s+=0x9E3779B9;
    }

    uint64_t total=0,min=~0ULL,max=0;
    uint32_t states[42]={0};
    uint32_t crc_acc=0;

    int unique_crc=0;
    uint32_t seen[56]={0};

    for(int i=0;i<56;i++){

        uint64_t t0=rdcycle();
        uint32_t c=crc32(buf,4096);
        uint64_t t1=rdcycle();

        uint64_t d=t1-t0;

        total+=d;
        if(d<min)min=d;
        if(d>max)max=d;

        crc_acc^=c;

        // estado 42
        int st=c%42;
        if(states[st]==0) states[st]=1;

        // unicidade
        int found=0;
        for(int k=0;k<i;k++) if(seen[k]==c) found=1;
        if(!found){ seen[i]=c; unique_crc++; }

        mix(buf);
    }

    // contagem estados
    int active=0;
    for(int i=0;i<42;i++) if(states[i]) active++;

    double avg=(double)total/56.0;
    double jitter=((double)(max-min))/avg;

    // entropia aproximada
    double p=(double)active/42.0;
    double entropy= -p*fast_log2(p);

    // =============================
    // OUTPUT
    // =============================
    out("\n==== VECTRA BENCH ====\n");

    out("CRC:"); hex64(crc_acc);

    out("\nTOTAL:"); hex64(total);
    out("\nMIN:"); hex64(min);
    out("\nMAX:"); hex64(max);

    out("\n");

    // métricas chave simplificadas
    out("ACTIVE STATES:");
    char c='0'+(active/10);
    syscall(SYS_write,1,&c,1);
    c='0'+(active%10);
    syscall(SYS_write,1,&c,1);

    out("\nUNIQUE CRC:");
    c='0'+(unique_crc/10);
    syscall(SYS_write,1,&c,1);
    c='0'+(unique_crc%10);
    syscall(SYS_write,1,&c,1);

    out("\n");

    return 0;
}
