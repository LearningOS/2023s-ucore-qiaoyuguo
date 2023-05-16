#include "syscall.h"
#include "console.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

//file type
#define DIR 0x040000
#define FILE 0x100000

typedef struct Stat{
	uint64 dev;
	uint64 ino;
	uint32 mode;
	uint32 nlink;
	uint64 pad[7];
}Stat;

uint64 console_write(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	tracef("write size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return len;
}

uint64 console_read(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	tracef("read size = %d", len);
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
}


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

uint64 sys_write(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d write\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_write(va, len);
	case FD_INODE:
		return inodewrite(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
}

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d read\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_read(va, len);
	case FD_INODE:
		return inoderead(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
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
	debugf("fork!");
	return fork();
}

static inline uint64 fetchaddr(pagetable_t pagetable, uint64 va)
{
	uint64 *addr = (uint64 *)useraddr(pagetable, va);
	return *addr;
}

uint64 sys_exec(uint64 path, uint64 uargv)
{
	struct proc *p = curr_proc();
	char name[MAX_STR_LEN];
	copyinstr(p->pagetable, name, path, MAX_STR_LEN);
	uint64 arg;
	static char strpool[MAX_ARG_NUM][MAX_STR_LEN];
	char *argv[MAX_ARG_NUM];
	int i;
	for (i = 0; uargv && (arg = fetchaddr(p->pagetable, uargv));
	     uargv += sizeof(char *), i++) {
		copyinstr(p->pagetable, (char *)strpool[i], arg, MAX_STR_LEN);
		argv[i] = (char *)strpool[i];
	}
	argv[i] = NULL;
	return exec(name, (char **)argv);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}

uint64 sys_spawn(uint64 va)
{
	struct inode *ip;
	struct proc *p = curr_proc();
	char name[200] = "";
	copyinstr(p->pagetable, name, va, 200);

	if ((ip = namei(name)) == 0) {
		errorf("invalid file name %s\n", name);
		return -1;
	}
	struct proc *np = allocproc();
	init_stdio(np);
	if (np == NULL) {
		panic("allocproc\n");
	}
	debugf("load proc %s", name);
	bin_loader(ip, np);
	iput(ip);
	char *argv[2];
	argv[0] = name;
	argv[1] = NULL;
	np->trapframe->a0 = push_argv(np, argv);
	np->parent = p;
	add_task(np);
	return np->pid;
}

uint64 sys_set_priority(long long prio)
{
	debugf("prio:%d %x", prio, prio);
	if(prio < 2 )
		return -1;
	struct proc *p = curr_proc();
	p->priority = prio;
    return prio;
}

uint64 sys_openat(uint64 va, uint64 omode, uint64 _flags)
{
	struct proc *p = curr_proc();
	char path[200];
	copyinstr(p->pagetable, path, va, 200);
	return fileopen(path, omode);
}

uint64 sys_close(int fd)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d close", fd);
		return -1;
	}
	fileclose(f);
	p->files[fd] = 0;
	return 0;
}

int sys_fstat(int fd, uint64 stat)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	struct Stat cstat;

	if(f->ip == 0) {
		warnf("fstat: failed to get inode\n");
		return -1;
	}
	memset(&cstat, 0, sizeof(cstat));
	cstat.dev = f->ip->dev;
	cstat.ino = f->ip->inum;
	cstat.nlink = f->ip->lc;
	cstat.mode = FILE;

	copyout(p->pagetable, stat, (char *)&cstat, sizeof(cstat));
	return 0;
}

int sys_linkat(int olddirfd, uint64 oldname, int newdirfd, uint64 newname,
	       uint64 flags)
{
	struct proc *p = curr_proc();
	char oldpath[200] = "";	
	copyinstr(p->pagetable, oldpath, oldname, 200);
	char newpath[200] = "";
	copyinstr(p->pagetable, newpath, newname, 200);

	if(strncmp(oldpath, newpath, 200) == 0) {
		warnf("linkat: newpath(%s) is same as oldpath(%s)\n", 
				newpath, oldpath);
		return -1;
	}
	return linkat(oldpath, newpath);
}

int sys_unlinkat(int dirfd, uint64 name, uint64 flags)
{
	struct proc *p = curr_proc();
	char path[200] = "";	
	copyinstr(p->pagetable, path, name, 200);
	
	return unlinkat(path);	
}


uint64 sys_sbrk(int n)
{
	uint64 addr;
	struct proc *p = curr_proc();
	addr = p->program_brk;
	if (growproc(n) < 0)
		return -1;
	return addr;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
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
	case SYS_openat:
		ret = sys_openat(args[0], args[1], args[2]);
		break;
	case SYS_close:
		ret = sys_close(args[0]);
		break;
	case SYS_exit:
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
		ret = sys_exec(args[0], args[1]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_fstat:
		ret = sys_fstat(args[0], args[1]);
		break;
	case SYS_linkat:
		ret = sys_linkat(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_unlinkat:
		ret = sys_unlinkat(args[0], args[1], args[2]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	case SYS_setpriority:
		ret = sys_set_priority(args[0]);
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
