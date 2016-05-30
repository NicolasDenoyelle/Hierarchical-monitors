#include "monitor_trace_interface.h"

extern hwloc_topology_t topology;

int *      monitor_trace_enum_events_field_indexes(FILE * trace, unsigned * n_events);
void       monitor_trace_seek_begin(FILE * trace){rewind(trace);}

#define next_token_begin(start, save_ptr) do{ start = strtok_r(NULL," ",&save_ptr); } while(start!=NULL && (*start==' ' || *start=='\n'))
#define handle_error(call,err,msg) if((call) == err){ perror(msg); exit(EXIT_FAILURE); }

int monitor_trace_parse_next_entry(FILE * trace, char * location, long * timestamp, double * values, int * trace_values_index){
    char * line = NULL;
    size_t n;
    if(getline(&line, &n, trace) == -1){
	if(feof(trace)){
	    return -1;
	}
	else{
	    perror("getline");
	    exit(EXIT_FAILURE);
	}
    }
    char * token, *save_ptr;    

    /* skip name */
    token = strtok_r(line," ", &save_ptr);

    /* read location */
    next_token_begin(token, save_ptr);    
    size_t location_max_len = strlen(location);
    size_t len = strlen(token) > location_max_len ? location_max_len : strlen(token);
    memset(location,0,location_max_len);
    strncpy(location,token,len);

    /* read timestamp */
    next_token_begin(token, save_ptr);    
    *timestamp = atol(token);

    int trace_index = 0, val_index = 0;
    while(token!=NULL){
	/* read value */
	next_token_begin(token, save_ptr);    
	if(token == NULL) break;
	if(trace_values_index[trace_index]){
	    values[val_index++] = atoll(token);
	}
	trace_index++;
    }
    free(line);
    return 0;
}

int monitor_trace_match_location_hwloc_obj(char * trace_location, hwloc_obj_t location_to_match){
    char save_ptr[32], * trace_location_type = strtok_r(trace_location,":",(char**)&save_ptr);
    char match_location_type[128]; hwloc_obj_type_snprintf(match_location_type, 128, location_to_match,0);
    if(strcmp(trace_location_type, match_location_type))
	return 0;
    return (int)location_to_match->logical_index == atoi(strtok_r(NULL,":",(char**)&save_ptr));
}

