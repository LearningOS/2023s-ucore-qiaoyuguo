#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
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

uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	uint64 cycle = get_cycle();
	TimeVal time;
	time.sec = cycle / CPU_FREQ;
	time.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	
	return copyout(curr_proc()->pagetable, (uint64)val, (char*)&time, sizeof(time));
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



// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
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

	return 0;
}
int sys_munmap(void* start, unsigned long long len)
{
	debugf("under sys_munmap, start=0x%x len=%d",
		   	(uint64)start, len);
	struct proc *p = curr_proc();

	int pages = ((len + PGSIZE - 1) >> PGSHIFT);
	uint64 end = (uint64)(start + (pages << PGSHIFT));
	uint64 tmp = (uint64)start;
	debugf("Starting to try to unmap %d pages from 0x%x", pages, (uint64)start);
	while(tmp < end)
	{
		if(walkaddr(p->pagetable, tmp) == 0)
		{
			debugf("0x%x is not mapped yet", tmp);
			return -1;
		}
		uvmunmap(p->pagetable, tmp, 1, 1);
		tmp += PGSIZE;
	}

	return 0;
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
{
	struct proc *p = curr_proc();	
	p->taskinfo.time = get_execute_time();

	return copyout(p->pagetable, (uint64)t, (char *)&p->taskinfo, sizeof(p->taskinfo));
}

extern char trap_page[];

void syscall()
{
	struct proc *proc = curr_proc();
	struct trapframe *trapframe = proc->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);

	proc->taskinfo.syscall_times[id] += 1;

	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		proc->taskinfo.time = get_execute_time();
		proc->taskinfo.status = Exited;
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
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
