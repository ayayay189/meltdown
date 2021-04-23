#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>


jmp_buf buf;
int samples = 100;
int i;
int a[1] = {1};
int test_illegal_address = 0;

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

static __inline__ void maccess(void *p) {
  asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax");
}

//main function for recovering from a segmentation fault
int main(int argc, char** argv) {
    if (signal(SIGSEGV, segfault_handler) == SIG_ERR) {
        printf("Failed to setup signal handler\n");
        return -1;
    }
    int nFault = 0;
    
    for(i = 0; i < samples; i++){
        test_illegal_address += 4096;

        if (!setjmp(buf)){
         // perform potentially invalid memory access
           maccess(&a[test_illegal_address]);
           continue;
        }
        nFault++;
    }
    printf("exception suppressed: %i\n", nFault);
}
