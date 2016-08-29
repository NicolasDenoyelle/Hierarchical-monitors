#include <pthread.h>
#include "../../internal.h"
#include "../performance_plugin.h"
#include "maqao.h"

/* Improvements :
 * Avoid false sharing of callsites;
 */
#define MAQAO_THREAD_ANY -1

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int nb_callsite = 0;

struct maqao_eventset{
    unsigned n_events;
    int core;
    unsigned int callsite;
};

inline uint64_t maqao_get_value(unsigned callsite, unsigned int eventIdx){
    return ((get_counter_info())[callsite][eventIdx])->value;
}

int __attribute__((constructor)) hmonitor_events_global_init(){
    pthread_mutex_init(&lock,NULL);
    return 0;
}
void __attribute__((destructor)) hmonitor_events_global_finalize(){
    pthread_mutex_destroy(&lock);
}

char ** hmonitor_events_list(int * n_events){
    char ** ret;

    *n_events = 1;
    malloc_chk(ret, sizeof(char*));
    *ret = strdup("See maqao doc for event list");
    return ret;
}

int hmonitor_eventset_add_named_event(void * monitor_eventset, const char * event){
    struct maqao_eventset * set = (struct maqao_eventset *) monitor_eventset;
    char * evt = strdup(event);
    switch(counting_add_hw_counters(evt , set->core, MAQAO_THREAD_ANY)){
    case -1:
	fprintf(stderr, "Maqao error: wrong event name\n");
	goto maqao_add_event_error;
	break;
    case -2:
	fprintf(stderr, "Maqao error: realloc error\n");
        goto maqao_add_event_error;
	break;
    default:
	break;
    }
    
    set->n_events++;
    free(evt);
    return 1;
    
maqao_add_event_error:
    free(evt);
    return -1;
}

int hmonitor_eventset_init(void ** eventset, hwloc_obj_t location){
    struct maqao_eventset * set;
    malloc_chk(set, sizeof(*set));
    set->n_events = 0;

    if(location->type == HWLOC_OBJ_PU){
	set->core = location->os_index;
    }
    else{
	//System wide
	set->core = MAQAO_THREAD_ANY;
	printf("Maqao eventset not bound to a processing unit. It will count events system wide.\n");
    }

    pthread_mutex_lock(&lock);
    set->callsite = nb_callsite++;
    pthread_mutex_unlock(&lock);

    *eventset = set;
    return 0;
}


static pthread_once_t once_control = PTHREAD_ONCE_INIT;
void hmonitor_eventset_init_fini_once(void){
    counting_start_counters(nb_callsite);
}

int hmonitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){
    pthread_once(&once_control,hmonitor_eventset_init_fini_once);
    return 0;
}

int hmonitor_eventset_destroy(void * eventset){
    free(eventset);
    return 0;
}

int hmonitor_eventset_start(void * eventset){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    counting_start_counting(set->callsite);
    return 0;
}

int hmonitor_eventset_stop(void * eventset){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    counting_stop_counting(set->callsite);
    return 0;
}

int hmonitor_eventset_reset(void * eventset){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    counting_stop_counting_dumb(set->callsite);
    pthread_mutex_lock(&lock);
    counting_start_counting(nb_callsite);
    pthread_mutex_unlock(&lock);
    return 0;
}

int hmonitor_eventset_read(void* eventset, double * values){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    unsigned int i = 0;
    for(i=0;i<set->n_events;i++)
	values[i] = maqao_get_value(set->callsite,i);
    return 0;
}
   
