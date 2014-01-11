#include <iostream>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include "uthreads_helpers.h"
#include "uthreads.h"

#define SYS_ERR true
#define MICRO_TO_SECOND 1000000
using namespace std;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr)); 
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;

#define JB_SP 4
#define JB_PC 5 

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
        "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct itimerval current_timer;
struct sigaction my_signal_action;
map<int, UThread*> threads;
vector<int> schedule_queue;         // list of ready threads
map<int,int> sleep_queue;           // the touple <tid, quantum_on_it_to_wake> of all threads that sleeping
int running_utid;
int total_qntm;
int next_avlbl_id;                  // the smallest non-occupied positive id.
int total_threads;

// returns the smallest positive id available.
static int get_available_id()
{
    int next_id = next_avlbl_id++;
    while(threads.find(next_avlbl_id)!=threads.end())
    {
        next_avlbl_id++;
    }    
    return next_id;
}

// clear all UThread allocations
static void clear_all_dbs()
{
    UThread* to_del;
    map<int, UThread*>::iterator it;    
    for(it = threads.begin();it!=threads.end(); it++)
    {
        to_del = it->second;                
        delete to_del;    
    }    
    threads.clear();
}

// waking up all sleeping threads that finish their amount of quantums to sleep
static void wakeup_from_sleep()
{
    map<int,int>::iterator iter;
    vector<int> del_from_squeue; 
    for(iter=sleep_queue.begin(); iter!=sleep_queue.end(); iter++)
    {
        if(iter->second == total_qntm) 
        {
            int utid_to_wake = iter->first;
            threads[utid_to_wake]->set_state(READY);
            schedule_queue.push_back(utid_to_wake);
            // to save the iterator from curroption, i keep the ids of all
            // the threads to delete and erase them later
            del_from_squeue.push_back(utid_to_wake);                      
        }
    }
    vector<int>::iterator it;
    for(it=del_from_squeue.begin(); it!=del_from_squeue.end(); it++)
    {
        sleep_queue.erase(*it);
    }
}

// reset the timer to the previous set values (initiate in uthread_init)
// update the total quantums counter, and wake up sleeping threads that reach their time.
static void set_timer()
{
    total_qntm++;
    wakeup_from_sleep();
    if(setitimer(ITIMER_VIRTUAL, &current_timer, NULL) == -1)
    {    
        handle_err("setitimer error- failed setting the timer.", SYS_ERR);
    }
}

// swiching to the next READY thread (which stored at the head of the schedule_queue).
static void run_next()
{    
    running_utid = schedule_queue.front();
    schedule_queue.erase(schedule_queue.begin());      
    
    threads[running_utid]->set_state(RUNNING);
    threads[running_utid]->qntm_elapse();      
    threads[running_utid]->load_env();
}

// handler function for the virtual timer signal. 
// that function reset the timer and switch to the next READY thread. 
static void qntm_expriration_handler(int signum)
{        
    set_timer();
    if(threads[running_utid]->save_env()) return;        
    threads[running_utid]->set_state(READY);
    schedule_queue.push_back(running_utid);    
    run_next();
}

// This function removes a thread id from the schedule_queue. if the given id is not in the 
// queue - does nothing. 
static void remove_from_schedule_queue(int utid_to_remove)
{
    vector<int>::iterator iter;
    for(iter = schedule_queue.begin(); iter != schedule_queue.end(); iter++)
    {
        if( (*iter) == utid_to_remove)
        {
            schedule_queue.erase(iter);
            return;
        }
    }
}

/* Initialize the thread library */
int uthread_init(int quantum_usecs)
{    
    my_signal_action.sa_handler = qntm_expriration_handler;
    if(sigemptyset (&my_signal_action.sa_mask) == -1)
    {
        handle_err("signal error- sigemptyset returned -1", SYS_ERR);
    }
    if(sigaddset(&my_signal_action.sa_mask, SIGVTALRM))
    {        
        handle_err("signal error- sigaddset returned -1", SYS_ERR);
    }    
    my_signal_action.sa_flags = 0;
    if(sigaction(SIGVTALRM,&my_signal_action,NULL) == -1)
    {
        handle_err("signal error- sigaction returned -1", SYS_ERR);
    }        
    
    if (quantum_usecs<=0)
    {
        total_threads = total_qntm = running_utid = -1;
        return handle_err("quantum_usecs must be positive number!");
    }
    
    if(0 == MAX_THREAD_NUM) 
    {
        return handle_err("Cant spawn the main because MAX_THREAD_NUM is zero.");
    }
    total_qntm = 0;
    running_utid = 0;    
    next_avlbl_id = 1;
    total_threads = 1;
    
    UThread *main_thread = new (std::nothrow) UThread();
    if(main_thread==NULL)
    {
        handle_err("failed to allocate memmory ('new' operator failed).", SYS_ERR);
    }
    threads[0] = main_thread;
    threads[0]->set_state(RUNNING);
    threads[0]->qntm_elapse();
    
    current_timer.it_value.tv_sec = current_timer.it_interval.tv_sec = (int)(quantum_usecs/MICRO_TO_SECOND);
    current_timer.it_value.tv_usec = current_timer.it_interval.tv_usec = quantum_usecs % MICRO_TO_SECOND;        
    set_timer();
    return 0;    
}

/* Create a new thread whose entry point is f */
int uthread_spawn(void (*f)(void))
{
    sigprocmask(SIG_BLOCK,&my_signal_action.sa_mask, NULL);
    int err_flag = 0;
    if(total_threads == -1) 
    {
        err_flag = handle_err("uthreads libary is not initialized.");
    }
    
    if(total_threads >= MAX_THREAD_NUM) 
    {
        err_flag = handle_err("Exceeds the maximum amount of threads.");
    }
    
    if((*f)==NULL)
    {
        err_flag = handle_err("The given function to spawn is NULL.");
    }
    if (err_flag == FAILURE_VAL)
    {
        sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
        return err_flag;
    }

    int new_id = get_available_id();    
    UThread *new_ut = new (std::nothrow) UThread();   
    if(new_ut==NULL)
    {
        handle_err("failed to allocate memmory ('new' operator failed).", SYS_ERR);
    }        
    threads[new_id] = new_ut;
    total_threads++;    
    
    schedule_queue.push_back(new_id);                
    address_t sp, pc;        
    sp = (address_t)new_ut->get_stack() + STACK_SIZE - sizeof(address_t);
    pc = (address_t)*f;
    sigjmp_buf& env = new_ut->get_env();
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);        
    if(sigemptyset(&env->__saved_mask) == -1)
    {
        handle_err("signal error- sigemptyset returned -1", SYS_ERR);
    }       
        
    sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
    return new_id;  
}

/* Terminate a thread */
int uthread_terminate(int tid)
{
    sigprocmask(SIG_BLOCK,&my_signal_action.sa_mask, NULL);    
    if(tid == 0)
    {
        clear_all_dbs();
        exit(0);
    }    
    
    if(threads.find(tid) == threads.end())
    {        
        sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
        return handle_err("Trying to terminates non-existing thread (wrong tid).");
    }    

    UThread *thread_to_terminate = threads[tid];
    remove_from_schedule_queue(tid);    
    if(thread_to_terminate->get_state() == SLEEPING)
    {
        sleep_queue.erase(tid);
    }
    
    total_threads--;    
    // if needed - keep next_avlbl_id the smallest valid id.
    if (next_avlbl_id>tid)
    {
        next_avlbl_id = tid;
    }
    threads.erase(tid);     
    delete thread_to_terminate; 
    if(tid == running_utid)
    {        
        set_timer();   
        run_next();           
    }
    
    sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
    return 0;
}

/* Suspend a thread */
int uthread_suspend(int tid)
{
    sigprocmask(SIG_BLOCK,&my_signal_action.sa_mask, NULL);
    int err_flag = 0;
    if(tid == 0)
    {        
        err_flag = handle_err("Can't suspend the main thread (suspend the tid 0).");
    }        
    
    if(threads.find(tid) == threads.end())
    {
        err_flag = handle_err("Can't suspend non-existing thread (suspend wrong tid)");
    }    
    
    if(err_flag == FAILURE_VAL)
    {
        sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
        return err_flag;
    }
    UThread *uthread = threads[tid];    
    if(uthread->get_state()!=SLEEPING && uthread->get_state()!=SUSPENDED)
    {
        remove_from_schedule_queue(tid);
        uthread->set_state(SUSPENDED);
        if(running_utid == tid)
        {            
            if(uthread->save_env())
            {
                return 0;
            }
            set_timer();
            run_next(); 
        }
    }
    
    sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
    return 0;
}

/* Resume a thread */
int uthread_resume(int tid)
{
    sigprocmask(SIG_BLOCK,&my_signal_action.sa_mask, NULL);
        
    if(threads.find(tid) == threads.end())
    {        
        sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
        return handle_err("Can't resume non-existing thread (resume wrong tid).");
    }
    
    UThread *uthread = threads[tid];
    
    if(uthread->get_state() == SUSPENDED)
    {
        uthread->set_state(READY);
        schedule_queue.push_back(tid);
    }
    sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
    return 0;
}

/* Put calling thread to sleep for specified number of quantums */
int uthread_sleep(int num_quantums)
{
    sigprocmask(SIG_BLOCK,&my_signal_action.sa_mask, NULL);
    int err_flag = 0;
    if (total_qntm==-1)
    {        
        err_flag = handle_err("uthreads libary is not initialized!");
    }
    
    if(running_utid == 0)
    {
        err_flag = handle_err("Can't put the main thread to sleep.");
    }    
    if(num_quantums <= 0)
    {
        err_flag = handle_err("Invalud number of quantum to sleep (non-positive num_quantums).");
    }
    if(err_flag == FAILURE_VAL)
    {
        sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
        return err_flag;
    }
    
    UThread *thread = threads[running_utid];
    thread->set_state(SLEEPING);
    remove_from_schedule_queue(running_utid);
    // store the thread id in the sleeping queue with the wake-up-value
    // the wake up time is relatively to the total_qntm already passed.
    // adding one because the current quantum is not counted.
    sleep_queue[running_utid] = total_qntm + num_quantums + 1;
    
    set_timer();
    if(thread->save_env()) 
    {
        return 0;
    } 
    run_next();   
    
    sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
    return 0;
}

/* Get the id of the calling thread */
int uthread_get_tid()
{
    return running_utid;
}

/* Get the total number of library quantums */
int uthread_get_total_quantums()
{
    return total_qntm;
}

/* Get the number of thread quantums */
int uthread_get_quantums(int tid)
{
    sigprocmask(SIG_BLOCK,&my_signal_action.sa_mask, NULL);
    int result;
    if(threads.find(tid)!=threads.end())
    {
        result = threads[tid]->get_qntms();
    }
    else
    {
        result = handle_err("uthread_get_quantums recieved wrong uthread-id.");
    }
    sigprocmask(SIG_UNBLOCK,&my_signal_action.sa_mask, NULL);
    return result;
}

