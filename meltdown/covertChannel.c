#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>


void clflush(volatile void* Tx) {
    asm volatile("lfence;clflush (%0) \n" :: "c" (Tx));
}

int space = 4096;
uint8_t * UserArray;
int data;
uint32_t *timing;
int threshold = 132;

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

int main(int argc, char** argv){
    int iter = 100;
    int correct = 0;
    UserArray = (uint8_t *) malloc(space * 256);
    
    for(int j = 0; j< iter; j++){
        data = random()%256;
        printf("data: %d\n", data);
    
        int i;
        int received_data;
    
        for(i = 0; i< 256; i++){
            clflush(&UserArray[i * space]);
        }
    
        maccess(&UserArray[data * space]);
    
        for(i = 0; i< 256; i++){
            //*timing = reload( &UserArray[i * space]);
            if(reload( &UserArray[i * space]) < threshold){
                received_data = i;
                
                //correct and only one
                if(received_data == data){
                    correct++;
                    printf("received_data: %d\n", received_data);
                    break;
                }
                
//                 else if(received_data > data){
//                     break;
//                 }
                
                
            }
        }
    }
    
    printf("correct: %d\n", correct);
    
}
