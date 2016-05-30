#include "maqao.h"
#include "../../monitor_utils.h"
#include <pthread.h>

/* Improvements :
 * Avoid false sharing of callsites;
 * Stop counting and accumulate instead of stop counting, yields an error
 */
#define MAQAO_THREAD_ANY -1

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int nb_callsite = 0;

struct maqao_eventset{
    unsigned n_events;
    int core;
    unsigned int callsite;
    void     (*stop_counting)(unsigned int);           //Function depend on whether we should accumulate or not.
    uint64_t (*get_value)(unsigned, unsigned int); //Function depend on whether we should accumulate or not.
};

inline uint64_t maqao_get_value(unsigned callsite, unsigned int eventIdx){
    return ((get_counter_info())[callsite][eventIdx])->value;
}

inline uint64_t maqao_get_value_accumulate(unsigned callsite, unsigned int eventIdx){
    return (get_counter_info_accumulate())[callsite][eventIdx];
}

int __attribute__((constructor)) monitor_events_global_init(){
    pthread_mutex_init(&lock,NULL);
    return 0;
}
void __attribute__((destructor)) monitor_events_global_finalize(){
    pthread_mutex_destroy(&lock);
}

char ** monitor_events_list(int * n_events){
    char ** ret;

    *n_events = 1;
    malloc_chk(ret, sizeof(char*));
    *ret = strdup("See maqao doc for event list");
    return ret;
}

int monitor_eventset_add_named_event(void * monitor_eventset, char * event){
    struct maqao_eventset * set = (struct maqao_eventset *) monitor_eventset;
    switch(counting_add_hw_counters(event , set->core, MAQAO_THREAD_ANY)){
    case -1:
	fprintf(stderr, "Maqao error: wrong event name\n");
	return -1;
	break;
    case -2:
	fprintf(stderr, "Maqao error: realloc error\n");
	return -1;
	break;
    default:
	break;
    }
    set->n_events++;
    return 1;
}

int monitor_eventset_init(void ** eventset, hwloc_obj_t location, int accumulate){
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
    if(accumulate){
	set->stop_counting = counting_stop_counting_and_accumulate;
	set->get_value = maqao_get_value_accumulate;
    }
    else{
	set->stop_counting = counting_stop_counting;
	set->get_value = maqao_get_value;
    }
    pthread_mutex_unlock(&lock);

    *eventset = set;
    return 0;
}


static pthread_once_t once_control = PTHREAD_ONCE_INIT;
void monitor_eventset_init_fini_once(void){
    counting_start_counters(nb_callsite);
}

int monitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){
    pthread_once(&once_control,monitor_eventset_init_fini_once);
    return 0;
}

int monitor_eventset_destroy(void * eventset){
    free(eventset);
    return 0;
}

int monitor_eventset_start(void * eventset){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    counting_start_counting(set->callsite);
    return 0;
}

int monitor_eventset_stop(void * eventset){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    set->stop_counting(set->callsite);
    return 0;
}

int monitor_eventset_reset(void * eventset){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    counting_stop_counting_dumb(set->callsite);
    pthread_mutex_lock(&lock);
    counting_start_counting(nb_callsite);
    pthread_mutex_unlock(&lock);
    return 0;
}

int monitor_eventset_read(void* eventset, long long * values){
    struct maqao_eventset * set = (struct maqao_eventset *) eventset;
    unsigned int i = 0;
    for(i=0;i<set->n_events;i++)
	values[i] = set->get_value(set->callsite,i);
    return 0;
}
   
