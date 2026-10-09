int flag;                          /* set by a signal handler or another thread */
volatile int vflag;

void wait_plain(void)    { while (!flag)  ; }
void wait_volatile(void) { while (!vflag) ; }
