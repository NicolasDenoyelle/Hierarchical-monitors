#include <sys/ptrace.h>
#include <string.h>
#include <hwloc.h>
#include <papi.h>
#include "../../monitor_utils.h"

extern hwloc_topology_t monitors_topology;

struct PAPI_eventset{
    int (*PAPI_stop_counting)(int, long long *);
    int evset;
    unsigned n_events;
    long long * values;
};

void
PAPI_handle_error(int err)
{
    if(err!=0)
	fprintf(stderr,"PAPI error %d: ",err);
    switch(err){
    case PAPI_EINVAL:
	fprintf(stderr,"Invalid argument.\n");
	break;
    case PAPI_ENOINIT:
	fprintf(stderr,"PAPI library is not initialized.\n");
	break;
    case PAPI_ENOMEM:
	fprintf(stderr,"Insufficient memory.\n");
	break;
    case PAPI_EISRUN:
	fprintf(stderr,"Eventset is already_counting events.\n");
	break;
    case PAPI_ECNFLCT:
	fprintf(stderr,"This event cannot be counted simultaneously with another event in the monitor eventset.\n");
	break;
    case PAPI_ENOEVNT:
	fprintf(stderr,"This event is not available on the underlying hardware.\n");
	break;
    case PAPI_ESYS:
	fprintf(stderr, "A system or C library call failed inside PAPI, errno:%s\n",strerror(errno)); 
	break;
    case PAPI_ENOEVST:
	fprintf(stderr,"The EventSet specified does not exist.\n");
	break;
    case PAPI_ECMP:
	fprintf(stderr,"This component does not support the underlying hardware.\n");
	break;
    case PAPI_ENOCMP:
	fprintf(stderr,"Argument is not a valid component. PAPI_ENOCMP\n");
	break;
    case PAPI_EBUG:
	fprintf(stderr,"Internal error, please send mail to the developers.\n");
	break;
    default:
	fprintf(stderr,"Unknown error ID, sometimes this error is due to \"/proc/sys/kernel/perf_event_paranoid\" not set to -1\n");
	break;
    }
}

#define PAPI_call_check(call, check_against, ret_val, ...) do{		\
	int PAPI_err = 0;						\
	if((PAPI_err = (call)) != (check_against)){			\
	    fprintf(stderr, __VA_ARGS__);				\
	    PAPI_handle_error(PAPI_err);				\
	    exit(EXIT_FAILURE);						\
	    return ret_val;						\
	}								\
    } while(0)


int __attribute__((constructor)) monitor_events_global_init(){
    PAPI_call_check(PAPI_library_init(PAPI_VER_CURRENT), PAPI_VER_CURRENT, -1, "PAPI library version mismatch: ");
    PAPI_call_check(PAPI_is_initialized(), PAPI_LOW_LEVEL_INITED, -1, "PAPI library init error: ");
    return 0;
}

void __attribute__((destructor)) monitor_events_global_finalize(){
}

#define check_count(avail, count, max_count) do{		\
	if(count == max_count-1){				\
	    max_count*=2;					\
	    realloc_chk(avail,sizeof(*avail)*max_count);	\
	}							\
    } while(0)


char ** monitor_events_list(int * n_events){
    unsigned count=0, max_count = PAPI_MAX_HWCTRS + PAPI_MAX_PRESET_EVENTS;
    char ** avail;
    int event_code = 0 | PAPI_NATIVE_MASK;
    PAPI_event_info_t info;
    int numcmp, cid;	
    int retval;

    malloc_chk(avail, sizeof(char*)*max_count);
    /* native events */
    numcmp = PAPI_num_components();
    for(cid = 0; cid < numcmp; cid++){
	const PAPI_component_info_t *component;
	component=PAPI_get_component_info(cid);
	if (component->disabled) continue;
	retval = PAPI_enum_cmp_event(&event_code, PAPI_ENUM_FIRST, cid);
	if(retval==PAPI_OK){
	    do{
		memset(&info, 0, sizeof(info));
		retval = PAPI_get_event_info(event_code, &info);
		if (retval != PAPI_OK) continue;
		avail[count]=strdup(info.symbol);
		check_count(avail,count,max_count);
		count++;
	    } while(PAPI_enum_cmp_event(&event_code, PAPI_ENUM_ALL, cid) == PAPI_OK);
	}
    }

    event_code = 0 | PAPI_PRESET_MASK;
    /* preset events */
    PAPI_enum_event( &event_code, PAPI_ENUM_FIRST );
    do {
	if ( PAPI_get_event_info( event_code, &info ) == PAPI_OK ) {
	    avail[count]=strdup(info.symbol);
	    check_count(avail,count,max_count);
	    count++;
	}
    } while (PAPI_enum_event( &event_code, PAPI_PRESET_ENUM_AVAIL ) == PAPI_OK);

    *n_events = count;
    return avail;  
}

int 
monitor_eventset_add_named_event(void * monitor_eventset, char * event)
{
    struct PAPI_eventset * evset = (struct PAPI_eventset *) monitor_eventset;
    PAPI_call_check(PAPI_add_named_event(evset->evset,event), PAPI_OK, -1, "Failed to add event %s to eventset: ",(char*)event);
    /* extend result array */
    evset->n_events++;
    realloc_chk(evset->values,evset->n_events*sizeof(*evset->values));
    evset->values[evset->n_events-1] =  0;
    return 1;
}

/*
 * NUMANode <--> perf_event
 * PU       <--> perf_event_uncore
 */
int PAPI_monitor_match_location_component(hwloc_obj_t location){
    int i, nb_cmp = PAPI_num_components();
    PAPI_component_info_t * info;
    for(i=0;i<nb_cmp;i++){
	info = (PAPI_component_info_t *)PAPI_get_component_info(i);
	if(!info->disabled){
	    //printf("%s %s\n",info->short_name, info->name);
	    if(
	       (location->type == HWLOC_OBJ_PU   && !strcmp(info->short_name,"perf")) ||
	       (location->type == HWLOC_OBJ_NODE && !strcmp(info->short_name,"peu"))
	       ){return i;}
	}
    }
    return -1;
}
    
int monitor_eventset_init(void ** eventset, hwloc_obj_t location, int accumulate){
    struct PAPI_eventset * evset;
    malloc_chk(evset, sizeof (*evset));
    *eventset = evset;
    evset->n_events=0;
    evset->evset = PAPI_NULL;
    evset->values = NULL;
    if(accumulate)
	evset->PAPI_stop_counting = PAPI_accum;
    else
	evset->PAPI_stop_counting = PAPI_stop;
    PAPI_call_check(PAPI_create_eventset(&evset->evset), PAPI_OK, -1, "Eventset creation failed: ");

    /* assign eventset to a component */
    int cidx = PAPI_monitor_match_location_component(location);
    if(cidx>=0 && cidx < PAPI_num_components()){
	PAPI_call_check(PAPI_assign_eventset_component(evset->evset, cidx), PAPI_OK, -1, "Failed to assign eventset to commponent: ");
    }
    
    if(cidx == 0){
	/* bind eventset to cpu */
	PAPI_option_t cpu_option;
	cpu_option.cpu.eventset=evset->evset;
	cpu_option.cpu.cpu_num = location->os_index;
	PAPI_call_check(PAPI_set_opt(PAPI_CPU_ATTACH,&cpu_option), PAPI_OK, -1, "Failed to bind eventset to cpu: ");
    }

    if(cidx < 0){
	char obj_str[128]; memset(obj_str,0,sizeof(obj_str)); hwloc_obj_type_snprintf(obj_str, sizeof(obj_str), location,1);
	printf("Warning PAPI eventset on obj %s is not assigned to a component. \n\
It will count for the whole system.\n", obj_str); 
    }
    return 0;
}

int monitor_eventset_destroy(void * eventset){
    if(eventset == NULL)
	return 0;
    struct PAPI_eventset * evset = (struct PAPI_eventset *) eventset;
    free(evset->values);
    PAPI_call_check(PAPI_cleanup_eventset(evset->evset), PAPI_OK, -1, "PAPI_cleanup_eventset error: ");
    PAPI_call_check(PAPI_destroy_eventset(&evset->evset), PAPI_OK, -1, "PAPI_destroy_eventset error: ");
    free(evset);
    return 0;
}

int monitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){
    return 0;
}


int monitor_eventset_start(void * eventset){
    struct PAPI_eventset * evset = (struct PAPI_eventset *) eventset;
    PAPI_call_check(PAPI_start(evset->evset), PAPI_OK, -1, "Eventset start failed: ");
    return 0;
}

#define PAPI_swap_ptr(a,b) do{void * tmp = a; a=b; b=tmp;} while(0)

int monitor_eventset_stop(void * eventset){
    struct PAPI_eventset * evset = (struct PAPI_eventset *) eventset;
    PAPI_call_check(PAPI_stop(evset->evset, evset->values), PAPI_OK, -1, "Eventset stop failed: ");
    return 0;
}

int monitor_eventset_reset(void * eventset){
    struct PAPI_eventset * evset = (struct PAPI_eventset *) eventset;
    PAPI_call_check(PAPI_reset(evset->evset), PAPI_OK, -1, "Eventset reset failed: ");
    return 0;
}

int monitor_eventset_read(void* eventset, long long * values){
    struct PAPI_eventset * evset = (struct PAPI_eventset *) eventset;
    int err = PAPI_read(evset->evset, values);
    if(err != PAPI_OK){
	PAPI_handle_error(err);
	return -1;
    }
    return 0;
}

