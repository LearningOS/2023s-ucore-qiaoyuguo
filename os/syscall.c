#include "syscall.h"
#include "console.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

int sys_mmap(void* start, unsigned long long len, int port, int flag, int fd)
{

	debugf("under sys_mmap, start=0x%x len=%d port=0x%x flag=0x%x, fd=%d",
		   	(uint64)start, len, port, flag, fd);
	struct proc *p = curr_proc();
	if((port & ~0x7) != 0)
	{
		debugf("port bits other than least 3 bits are set, port = 0x%x", port);
		return -1;
	}
	if((port & 0x7) == 0)
	{
		debugf("all lowest 3 bits of port are 0");
		return -1;
	}
	if(!PGALIGNED((uint64)start))
	{
		debugf("0x%x is not aligned to page address", (uint64)start);
		return -1;
	}
	
	if(len == 0)
		return 0;
	if(len > 0x1000000000ULL)
	{
		debugf("len:%d is too big", len);
		return -1;
	}
	int pages = ((len + PGSIZE - 1) >> PGSHIFT);
	debugf("checking if %d pages from 0x%x is already mapped", pages, start);
	uint64 end = (uint64)(start + (pages << PGSHIFT));
	uint64 tmp = (uint64)start;
	while(tmp < end)
	{
		if(walkaddr(p->pagetable, tmp) != 0)
		{
			debugf("tmp(0x%x) is already mapped", tmp);
			return -1;
		}
		tmp += PGSIZE;
	}

	(void)fd;
	(void)flag;
	debugf("Starting to allocate and map %d pages", pages);
	tmp = (uint64)start;
	while(tmp < end)
	{
		uint64 phyaddr = (uint64)kalloc();
		if(phyaddr == 0)
		{
			debugf("Failed to allocate new page");
			return -1;
		}
		debugf("allocate page from kernel successfully");
		if(-1 == mappages(p->pagetable, tmp, PGSIZE, phyaddr, PTE_U | (port << 1)))
		{
			debugf("Failed to map new page to 0x%x", tmp);
			return -1;
		}
		debugf("map allocated page to 0x%x successfully", tmp);
		tmp += PGSIZE;	
	}

	int map_index = -1;
	for(int i = 0; i < NELEM(p->map); i++)
	{
		if(p->map[i].start == 0)
		{
			map_index = i;
			break;
		}
	}
	if(map_index != -1)
	{
		p->map[map_index].start = (uint64)start;
		p->map[map_index].length = len;
	}

	return 0;
}
int sys_munmap(void* start, unsigned long long len)
{
	return munmap(start, len);
}
static int get_execute_time(void)
{
	int execute_time = 0;
	struct proc *p = curr_proc();
	int pass_time = get_cycle() - p->startime;
	execute_time += pass_time / CPU_FREQ * 1000;
	execute_time += (pass_time % CPU_FREQ) * 1000 / CPU_FREQ;
	return execute_time;
}

int sys_task_info(TaskInfo *t)
{ struct proc *p = curr_proc();	
	p->taskinfo.time = get_execute_time();

	debugf("taskinfo time = %d", p->taskinfo.time);
	return copyout(p->pagetable, (uint64)t, (char *)&p->taskinfo, sizeof(p->taskinfo));
}

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d str = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	debugf("sys_read fd = %d str = %x, len = %d", fd, va, len);
	if (fd != STDIN)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(uint64 val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	TimeVal t;
	t.sec = cycle / CPU_FREQ;
	t.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(p->pagetable, val, (char *)&t, sizeof(TimeVal));
	return 0;
}

uint64 sys_getpid()
{
	return curr_proc()->pid;
}

uint64 sys_getppid()
{
	struct proc *p = curr_proc();
	return p->parent == NULL ? IDLE_PID : p->parent->pid;
}

uint64 sys_clone()
{
	debugf("fork!\n");
	return fork();
}

uint64 sys_exec(uint64 va)
{
	struct proc *p = curr_proc();
	char name[200];
	copyinstr(p->pagetable, name, va, 200);
	debugf("sys_exec %s\n", name);
	return exec(name);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}

uint64 sys_spawn(uint64 va)
{
	// TODO: your job is to complete the sys call
	return -1;
}

uint64 sys_set_priority(long long prio){
    // TODO: your job is to complete the sys call
    return -1;
}


uint64 sys_sbrk(int n)
{
        uint64 addr;
        struct proc *p = curr_proc();
        addr = p->program_brk;
        if(growproc(n) < 0)
                return -1;
        return addr;
}

extern char trap_page[];

void syscall()
{
	struct proc *proc = curr_proc();
	struct trapframe *trapframe = proc->trapframe;
	int id = trapframe->a7, ret;
	proc->taskinfo.syscall_times[id] += 1;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_read:
		ret = sys_read(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		proc->taskinfo.time = get_execute_time();
		proc->taskinfo.status = Exited;
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_clone: // SYS_fork
		ret = sys_clone();
		break;
	case SYS_execve:
		ret = sys_exec(args[0]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]);
		break;
	case SYS_mmap:
		ret = sys_mmap((void*)args[0], (unsigned long long)args[1], (int)args[2], (int)args[3], (int)args[4]);
		break;
	case SYS_munmap:
		ret = sys_munmap((void*)args[0], (unsigned long long)args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
