#define _GNU_SOURCE
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <arm_neon.h>

#define C 8
#define N 512
#define I 200000

typedef struct { float32x4_t s[8]; float32x4_t f; uint64_t a; uint8_t p[24]; } __attribute__((aligned(64))) X;

static X m[C][N];

static void* w(void* arg){
    int c=*(int*)arg;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(c,&set);
    sched_setaffinity(0,sizeof(set),&set);
    float32x4_t pi=vdupq_n_f32(3.1415927f), ph=vdupq_n_f32(1.6180339f), fc=vdupq_n_f32(0.001f);
    for(int t=0;t<I;t++){
        for(int i=0;i<N;i++){
            X* x=&m[c][i];
            __builtin_prefetch(x+2,1,3);
            for(int s=0;s<8;s++){
                x->s[s]=vmlaq_f32(x->s[s],x->s[(s+1)&7],pi);
                x->s[s]=vmlsq_f32(x->s[s],x->s[(s+3)&7],ph);
            }
            x->f=vaddq_f32(x->f,fc);
            uint64_t tor=vgetq_lane_u64(vreinterpretq_u64_f32(x->s[0]),0);
            x->a=__crc32d(x->a,tor);
        }
    }
    return 0;
}

int main(){
    for(int c=0;c<C;c++)
        for(int i=0;i<N;i++){
            X* x=&m[c][i];
            for(int s=0;s<8;s++) x->s[s]=vdupq_n_f32(c*1000.0f+i*0.1f+s*0.001f);
            x->a=0x9E3779B97F4A7C15ULL;
        }

    pthread_t th[C];
    int id[C];
    struct timespec st,en;
    clock_gettime(CLOCK_MONOTONIC,&st);
    for(int i=0;i<C;i++){ id[i]=i; pthread_create(&th[i],0,w,&id[i]); }
    for(int i=0;i<C;i++) pthread_join(th[i],0);
    clock_gettime(CLOCK_MONOTONIC,&en);
    double e=(en.tv_sec-st.tv_sec)+(en.tv_nsec-st.tv_nsec)/1e9;
    uint64_t p=(uint64_t)C*N*I, s=p*32;

    // Métricas mínimas
    double mp=p/e/1e6, gs=s/e/1e9, lat=(e*1e9)/p, gflops=(p*16.0/e)/1e9;
    write(1,"\nTOP METRICS:\n",13);
    char buf[256];
    int n=sprintf(buf,"• Throughput Vetorial: %.2f M-Pulses/s\n",mp); write(1,buf,n);
    n=sprintf(buf,"• Throughput Estados: %.2f G-States/s\n",gs); write(1,buf,n);
    n=sprintf(buf,"• Latência média: %.2f ns\n",lat); write(1,buf,n);
    n=sprintf(buf,"• NEON FLOPS: %.2f GFlop/s\n",gflops); write(1,buf,n);

    uint64_t v=0;
    for(int c=0;c<C;c++) v^=m[c][0].a;
    n=sprintf(buf,"• Âncora Global: 0x%016llX\n", (unsigned long long)v); write(1,buf,n);
    return 0;
}
