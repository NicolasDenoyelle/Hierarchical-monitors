#include <signal.h>
#include <sys/time.h>
#include "./hmon.h"

int handler_isset = 0;
timer_t update_timer;


timer_t display_timer;
int display_arg;
int (*display_function)(int);

void __attribute__ ((constructor)) hmon_block_SIGRTMIN(){
  /* avoid that signals are caught by threads */
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGRTMIN);
  if(pthread_sigmask(SIG_BLOCK, &set, NULL) == -1){perror("pthread_sigmask");}
}

static void handler(int sig, siginfo_t *si, void *uc){
  timer_t * timer = si->si_value.sival_ptr;
  if(sig == SIGRTMIN){
    if(*timer == update_timer){
	hmon_update();
    }
    if(*timer == display_timer){display_function(display_arg);}
  }
}

static int set_handler(){
  /* Allow this thread to catch SIGRTMIN */
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGRTMIN);
  if(pthread_sigmask(SIG_UNBLOCK, &set, NULL) == -1){perror("pthread_sigmask"); return -1;}

   /* register handler for SIGRTMIN */
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigemptyset(&sa.sa_mask);
  if(sigaction(SIGRTMIN, &sa, NULL) == -1){perror("sigaction"); return -1;}
  handler_isset = 1;
  return 0;
}

static int create_timer(timer_t * timerid){
/* Create the timer */
  struct sigevent sev;
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGRTMIN;
  sev.sigev_value.sival_ptr = timerid;
  if(timer_create(CLOCK_REALTIME, &sev, timerid) == -1){perror("timer_create"); return -1;}
}

static int set_timer(timer_t timer, long us){
  struct itimerspec update_delay;
  update_delay.it_interval.tv_sec  = us/1000000;
  update_delay.it_interval.tv_nsec = 1000*(us%1000000);
  update_delay.it_value = update_delay.it_interval;
  if(timer_settime(timer, 0, &update_delay, NULL) == -1){perror("timer_settime"); return -1;}
  return 0;
}

int hmon_sampling_start(const long us){
  if(!handler_isset && set_handler() == -1){return -1;}
  if(create_timer(&update_timer) == -1){return -1;}
  return set_timer(update_timer, us);
}

int hmon_sampling_stop(){
  set_timer(update_timer, 0);
  if(timer_delete(update_timer) == -1){perror("timer_delete"); return -1;}
  return 0;
}

int hmon_periodic_display_start(int (*display_monitors)(int), int arg){
  display_function = display_monitors;
  display_arg = arg;
  if(!handler_isset && set_handler() == -1){return -1;}
  if(create_timer(&display_timer) == -1){return -1;}
  return set_timer(display_timer, 100000);
}

int hmon_periodic_display_stop(){
  set_timer(display_timer, 0);
  if(timer_delete(display_timer) == -1){perror("timer_delete"); return -1;}
  return 0;
}

