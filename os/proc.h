#ifndef PROC_H
#define PROC_H

#include "riscv.h"
#include "types.h"
#include "queue.h"

#define NPROC (512)
#define FD_BUFFER_SIZE (16)

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
}TaskStatus;

#define MAX_SYSCALL_NUM 500
typedef struct TaskInfo{
	TaskStatus status;
	unsigned int syscall_times[MAX_SYSCALL_NUM];
	int time;
}TaskInfo;

#define MAX_MAP 5
typedef struct MapInfo{
	uint64 start;
	unsigned long long length;	
}MapInfo;

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
	uint64 program_brk;
	uint64 heap_bottom;
	TaskInfo taskinfo;
	MapInfo map[MAX_MAP];
	uint64 startime;
	int firstaccess;
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
// swtch.S
void swtch(struct context *, struct context *);

int growproc(int n);
int munmap(void* start, unsigned long long len);

#endif // PROC_H
