#ifndef _UTHREADS_HELPERS_H
#define _UTHREADS_HELPERS_H


#include <setjmp.h>
#include "uthreads.h"

// list of all thread's states:
#define RUNNING 1
#define READY 2
#define SUSPENDED 3
#define SLEEPING 4

#define FAILURE_VAL -1

// User Thread Class
class UThread
{  
    public:
        UThread();
        ~UThread();
        int get_state() const;
        const char* get_stack();
		sigjmp_buf& get_env();
		void set_state(int new_state);		
		bool save_env();
		void load_env();
		void qntm_elapse();
		int get_qntms() const;
    
    private:  
        char stack[STACK_SIZE];
        int state;
        int total_qntm;        
        sigjmp_buf env;
        
};


// Handles errors reports as describe in ex2 description:
// prints the suitable prefix to the error massage according to the is_system argument, and
// appends to that the given msg string.
// returning the predefine FAILURE_VAL if is_system is false, else: exiting with "exit(1)"
int handle_err(std::string msg, bool is_system=false);

#endif

