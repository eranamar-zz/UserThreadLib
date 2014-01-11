#include <iostream>
#include <signal.h>
#include <cstdlib>
#include "uthreads_helpers.h"

using namespace std;

// initialize a new uthread
UThread::UThread()
{
    total_qntm = 0;
	state = READY;
}

// destructor for the uthread - empty because there is no need to release anything.
UThread::~UThread()
{
}

// get the uthread's state
int UThread::get_state() const
{
	return state;
}

// get the uthread's stack
const char* UThread::get_stack()
{
	return stack;
}

// get the uthread's enviroment
sigjmp_buf& UThread::get_env()
{
	return env;
}

// set new state to the uthread 
void UThread::set_state(int new_state)
{
	state = new_state;
}

// save the entire uthread enviroment
bool UThread::save_env() 
{
	int ret_val = sigsetjmp(env,1);
	return ret_val;
}

// load the uthread's enviroment
void UThread::load_env(){
	siglongjmp(env,1);
}

// increase the total quantums elapsed counter
void UThread::qntm_elapse() 
{
    total_qntm++;
}

// get the total quantum elapsed during the runnign time of this thread
int UThread::get_qntms() const
{
    return total_qntm;
}

// Handles errors reports as describe in ex2 description:
// prints the suitable prefix to the error massage according to the is_system argument, and
// appends to that the given msg string.
// returning the predefine FAILURE_VAL if is_system is false, else: exiting with "exit(1)"
int handle_err(std::string msg, bool is_system)
{
    if(is_system)
    {
        cerr<<"system error: "<<msg<<endl;
        exit(1);        
    }
    else
    {
        cerr<<"thread library error: "<<msg<<endl;        
    }
    
    return FAILURE_VAL;
}
