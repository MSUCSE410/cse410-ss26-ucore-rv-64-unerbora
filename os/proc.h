// proj 3 expl
// (proc.h, proc.c, syscall.c)
// b4 procs run one after another --> bad w no mgmt
// we turn it -->multiproc os
// we already had fork, exec --> fork copies all mem, exec - child throws away the clone
// spawn combines two: f,e
// alloc fresh proc n load prog into it (witho touch parent mem)
// ---------------------------
// resrc mgmt - with spawn he have many procs run concurrently.
// when multiple procs run, os needs to manage rsrc.
// init, sched used fifo not useful. 
// 
// if not policy, some procs could starve.
// sched algo --> proc with p10 gets 2x cpu time to proc w p5.
// called stride sched. 


#ifndef PROC_H
#define PROC_H

#include "riscv.h"
#include "types.h"
#include "queue.h"

#define NPROC (512)
#define FD_BUFFER_SIZE (16)
#define MAX_SYSCALL_NUM (500)
#define BIG_STRIDE (1LL << 30)//stride cons (1 biln) overflow safety added
// 

struct file;

// Saved registers for kernel context switches.
struct context {
	uint64 ra;
	uint64 sp;

	// callee-saved
	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
	uint64 s9;
	uint64 s10;
	uint64 s11;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
typedef enum {
	UnInit,
	Ready,
	Running,
	Exited,
} TaskStatus;

typedef struct TaskInfo {
	TaskStatus status;
	unsigned int syscall_times[MAX_SYSCALL_NUM];
	int time;
} TaskInfo;

// Per-process state
struct proc {
	enum procstate state; // Process state
	int pid; // Process ID
	pagetable_t pagetable; // User page table
	uint64 ustack; // Virtual address of kernel stack
	uint64 kstack; // Virtual address of kernel stack
	struct trapframe *trapframe; // data page for trampoline.S
	struct context context; // swtch() here to run process
	uint64 max_page;
	struct proc *parent; // Parent process
	uint64 exit_code;
	struct file *files[FD_BUFFER_SIZE];
	unsigned int syscall_times[MAX_SYSCALL_NUM]; 
	uint64 start_time;
	//track stride and priority
	long long stride; // init 0
    long long priority; // init 16
};

int cpuid();
struct proc *curr_proc();
void exit(int);
void proc_init();
void scheduler() __attribute__((noreturn));
void sched();
void yield();
int fork();
int exec(char *);
int wait(int, int *);
void add_task(struct proc *);
struct proc *pop_task();
struct proc *allocproc();
int fdalloc(struct file *);
int spawn(char *name);
// swtch.S
void swtch(struct context *, struct context *);

#endif // PROC_H