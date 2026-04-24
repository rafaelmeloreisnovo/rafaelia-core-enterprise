#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>

// ============================================
// TIMER UNIVERSAL (fallback + ARM64 cycle)
// ============================================
static inline uint64_t rdtime() {
#if defined(aarch64)
uint64_t v;
asm volatile("mrs %0, cntvct_el0" : "=r"(v));
if (v != 0) return v;
#endif
struct timespec t;
clock_gettime(CLOCK_MONOTONIC, &t);
return (uint64_t)t.tv_sec * 1000000000ULL + t.tv_nsec;
}

// ============================================
// WRITE DIRETO (sem printf)
// ============================================
static void out(const char *s){
int n=0; while(s[n]) n++;
syscall(SYS_write,1,s,n);
}

static void hex64(uint64_t v){
char h[]="0123456789abcdef";
char o[16];
for(int i=15;i>=0;i--){ o[i]=h[v&0xF]; v>>=4; }
syscall(SYS_write,1,o,16);
}

// ============================================
// CRC UNIVERSAL (invariante)
// ============================================
static uint32_t crc32(uint8_t *p, int n){
uint32_t c=0;
for(int i=0;i<n;i++){
c^=p[i];
for(int j=0;j<8;j++)
c=(c>>1)^(0xEDB88320 & -(c&1));
}
return c;
}

// ============================================
// MIX UNIVERSAL (variante)
// ============================================
static void mix(uint8_t *p){
for(int i=0;i<4096;i+=64){
for(int j=0;j<64;j++){
uint8_t a=p[i+((j+7)&63)];
uint8_t b=p[i+((j+11)&63)];
p[i+j]^=a^b;
}
}
}

// ============================================
// LOG2 APROX (sem libc pesada)
// ============================================
static double fast_log2(double x){
int e;
double m = frexp(x, &e);
return (double)e + (m - 0.70710678);
}

// ============================================
// MAIN
// ============================================
int main(){

static uint8_t buf[4096];

// init determinístico
uint64_t s=0x9E3779B97F4A7C15ULL;
for(int i=0;i<4096;i+=8){
    *(uint64_t*)(buf+i)=s;
    s+=0x9E3779B9;
}

// =============================
// PREAQUECIMENTO (descarta)
// =============================
for(int i=0;i<16;i++){
    crc32(buf,4096);
    mix(buf);
}

// =============================
// COLETA
// =============================
uint64_t total=0,min=~0ULL,max=0;
uint32_t states[42]={0};
uint32_t crc_acc=0;

int unique_crc=0;
uint32_t seen[56]={0};

uint64_t deltas[56];

for(int i=0;i<56;i++){

    uint64_t t0=rdtime();
    uint32_t c=crc32(buf,4096);
    uint64_t t1=rdtime();

    uint64_t d=t1-t0;
    deltas[i]=d;

    total+=d;
    if(d<min)min=d;
    if(d>max)max=d;

    crc_acc^=c;

    int st=c%42;
    if(states[st]==0) states[st]=1;

    int found=0;
    for(int k=0;k<i;k++) if(seen[k]==c) found=1;
    if(!found){ seen[i]=c; unique_crc++; }

    mix(buf);
}

// =============================
// ESTADOS
// =============================
int active=0;
for(int i=0;i<42;i++) if(states[i]) active++;

// =============================
// MÉTRICAS
// =============================
double avg=(double)total/56.0;
double jitter=((double)(max-min))/avg;

double density=(double)active/42.0;
double diversity=(double)unique_crc/56.0;

double entropy = -density * fast_log2(density);

double throughput = (4096.0*56.0)/(total/1e9); // bytes/s

double efficiency = (entropy * active * diversity) / (avg * (1.0 + jitter));

// =============================
// OUTPUT
// =============================
out("\n==== VECTRA FINAL BENCH ====\n");

out("CRC:"); hex64(crc_acc);

out("\nTOTAL:"); hex64(total);
out("\nMIN:"); hex64(min);
out("\nMAX:"); hex64(max);

out("\n");

out("ACTIVE:");
char c='0'+(active/10); syscall(SYS_write,1,&c,1);
c='0'+(active%10); syscall(SYS_write,1,&c,1);

out("\nUNIQUE:");
c='0'+(unique_crc/10); syscall(SYS_write,1,&c,1);
c='0'+(unique_crc%10); syscall(SYS_write,1,&c,1);

out("\n");

// score simplificado
out("SCORE:");
hex64((uint64_t)(efficiency * 1000000));

out("\n===========================\n");

return 0;

}
