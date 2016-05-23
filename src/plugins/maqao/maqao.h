#include <inttypes.h>

 //INIT FUNCTIONS                                                                
 // CHOOSE YOUR HARDWARE COUNTERS (list separated by comma)
 // CORE : SELECT THE CORE (SYSTEM WILD)
 // PID  : SELECT THE PID
 unsigned int counting_add_hw_counters(char* hwcList, int core, int pid);                           

 // nbCallSites = NUMBER OF PROBES
 void counting_start_counters(unsigned int nbCallSites);                         
                                                                                 
 //START & STOP FUNCTIONS                                                        
 void counting_start_counting (unsigned int callSiteId);                          
 void counting_stop_counting (unsigned int callSiteId);                          
 void counting_stop_counting_and_accumulate (unsigned int callSiteId);           
 void counting_stop_counting_dumb (unsigned int callSiteId);                          
 void counting_stop_counting_and_accumulate_dumb (unsigned int callSiteId);           
                                                                                 
 //DUMP FUNCTIONS                                                                
 void counting_dump_file (char* fileName);                                       
 void counting_dump_file_by_line (char* fileName);                               
 void counting_dump_file_accumulate (char* fileName);                            
 char** counting_dump (void);                                                    
 char*** counting_dump_accumulate (void);                                        

// LProf LIBRARY STRUCT FOR COUNTER INFORMATION
typedef struct counterInfo                                                      
{                                                                               
   char* name;                  // HWC NAME                                     
   int id;                      // CALL SITE ID                                 
   uint64_t value;              // HWC VALUE                                    
}ScounterInfo;                                                                  

// GET FUNCTIONS
uint64_t** get_counter_info_accumulate();
ScounterInfo*** get_counter_info ();
unsigned int get_nb_callsites ();
unsigned int get_nb_counters ();

