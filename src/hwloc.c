#include "monitor.h"

static int chk_cpu_bind(hwloc_cpuset_t cpuset, int print)
{
    hwloc_bitmap_t checkset = hwloc_bitmap_alloc();
    if(hwloc_get_cpubind(monitor_topology, checkset, HWLOC_CPUBIND_THREAD) == -1){
	perror("get_cpubind");
	hwloc_bitmap_free(checkset);  
	return -1; 
    }
    if(print){
	hwloc_obj_t cpu_obj = hwloc_get_first_largest_obj_inside_cpuset(monitor_topology, checkset);
	printf("cpubind=%s:%d\n",hwloc_obj_type_string(cpu_obj->type),cpu_obj->logical_index);
    }

    if(cpuset == NULL)
	return -1;
    int ret = hwloc_bitmap_isequal(cpuset,checkset);
    hwloc_bitmap_free(checkset);  
    return ret ? 0 : -1;
}

static int chk_mem_bind(hwloc_nodeset_t nodeset, int print)
{
    hwloc_membind_policy_t policy;
    hwloc_bitmap_t checkset = hwloc_bitmap_alloc();
    if(hwloc_get_membind(monitor_topology, checkset, &policy, HWLOC_MEMBIND_THREAD|HWLOC_MEMBIND_BYNODESET) == -1){
	perror("get_membind");
	hwloc_bitmap_free(checkset);  
	return -1; 
    }
    if(print){
	char * policy_name;
	switch(policy){
	case HWLOC_MEMBIND_DEFAULT:
	    policy_name = "DEFAULT";
	    break;
	case HWLOC_MEMBIND_FIRSTTOUCH:
	    policy_name = "FIRSTTOUCH";
	    break;
	case HWLOC_MEMBIND_BIND:
	    policy_name = "BIND";
	    break;
	case HWLOC_MEMBIND_INTERLEAVE:
	    policy_name = "INTERLEAVE";
	    break;
	case HWLOC_MEMBIND_NEXTTOUCH:
	    policy_name = "NEXTTOUCH";
	    break;
	case HWLOC_MEMBIND_MIXED:
	    policy_name = "MIXED";
	    break;
	default:
	    policy_name=NULL;
	    break;
	}
	hwloc_obj_t mem_obj = hwloc_get_first_largest_obj_inside_cpuset(monitor_topology, checkset);
	printf("membind(%s)=%s:%d\n",policy_name,hwloc_obj_type_string(mem_obj->type),mem_obj->logical_index);
    }

    if(nodeset == NULL)
	return -1;
    int ret = hwloc_bitmap_isequal(nodeset,checkset);
    hwloc_bitmap_free(checkset);  
    return ret ? 0 : -1;
}

char * location_name(hwloc_obj_t obj){
    char * ret = malloc(64); 
    memset(ret,0,64);
    snprintf(ret, 63, "%s:%u", hwloc_type_name(obj->type), obj->logical_index);
    return ret;
}

hwloc_obj_t location_parse(char* location){
    hwloc_obj_type_t type; 
    char * name;
    int err, depth; 
    char * idx;
    int logical_index;

    name = strtok(location,":");
    if(name==NULL){return NULL;}
    err = hwloc_type_sscanf_as_depth(name, &type, monitor_topology, &depth);
    if(err == HWLOC_TYPE_DEPTH_UNKNOWN){
	fprintf(stderr,"type %s cannot be found, level=%d\n",name,depth);
	return NULL;
    }
    if(depth == HWLOC_TYPE_DEPTH_MULTIPLE){
	fprintf(stderr,"type %s multiple caches match for\n",name);
	return NULL;
    }
    logical_index = 0;
    idx = strtok(NULL,":");
    if(idx!=NULL){logical_index = atoi(idx);}
    return hwloc_get_obj_by_depth(monitor_topology,depth,logical_index);
}

int location_cpubind(hwloc_obj_t location)
{
    if(hwloc_set_cpubind(monitor_topology,location->cpuset, HWLOC_CPUBIND_THREAD|HWLOC_CPUBIND_STRICT|HWLOC_CPUBIND_NOMEMBIND) == -1){
	perror("cpubind");
	return -1;
    }
    return chk_cpu_bind(location->cpuset,0);
}

int location_membind(hwloc_obj_t location)
{
    while(location != NULL && location->type != HWLOC_OBJ_NODE)
	location = location ->parent;
    if(location == NULL){
	char * name = location_name(location);
	fprintf(stderr, "Cannot bind memory to %s\n", name);
	free(name);
	return -1;
    }

    if(hwloc_set_membind(monitor_topology, location->nodeset, HWLOC_MEMBIND_BIND,HWLOC_MEMBIND_THREAD | HWLOC_MEMBIND_BYNODESET) == -1){
	perror("membind");
	return -1;
    }

    return chk_mem_bind(location->nodeset, 0);
}

char ** location_avail(unsigned * nobjs){
    int depth;
    hwloc_obj_t obj;
    char ** obj_types;
    char obj_type[128]; 
  
    depth = hwloc_topology_get_depth(monitor_topology);
    obj_types=malloc(sizeof(char*)*depth);
    int i,index=0;
  
    obj = hwloc_get_root_obj(monitor_topology);
    do{
	memset(obj_type,0,128);
	hwloc_obj_type_snprintf(obj_type, 128, obj, 0);

	for(i=0;i<index;i++){
	    if(!strcmp(obj_type,obj_types[i])){
		break;
	    }
	}
	if(i==index){
	    obj_types[index] = strdup(obj_type);
	    index++;
	}
    } while((obj=hwloc_get_next_child(monitor_topology,obj,NULL))!=NULL);

    *nobjs=index;
    hwloc_topology_destroy(monitor_topology);
    return obj_types;
}

int location_compare(void * a, void * b){
    hwloc_obj_t obj_a = *(hwloc_obj_t *)a;
    hwloc_obj_t obj_b = *(hwloc_obj_t *)b;
    if(obj_a->depth < obj_b->depth)
	return 1;
    else if(obj_a->depth > obj_b->depth)
	return -1;
    else if(obj_a->depth == obj_b->depth){
	if(obj_a->logical_index > obj_b->logical_index)
	    return 1;
	else if(obj_a->logical_index < obj_b->logical_index)
	    return -1;
	else if(obj_a->logical_index == obj_b->logical_index)
	    return 0;
    }
    return 0;
}

