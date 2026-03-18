#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

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

uint64 sys_gettimeofday(TimeVal *val, int _tz)
{
	TimeVal *physical_val = (TimeVal *)useraddr(curr_proc()->pagetable, (uint64)val);
	if (physical_val == 0)
		return -1;

	uint64 cycle = get_cycle();
	physical_val->sec = cycle / CPU_FREQ;
	physical_val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
//this solves the problem that what if the program needs more memory at runtime?
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd)
{
    // len 0 is valid
    if (len == 0) return 0;

    //validate upper bits must be 0 lower bits cant all be 0
    if ((port & ~0x7) != 0) return -1;
    if ((port & 0x7) == 0) return -1;
    // start must be aligned means multiple of 4096 cant get half page
    if (start % PGSIZE != 0) return -1;
    // len < 1GB cant go larger than that
    if (len > (1u << 30)) return -1;

    //page boundary round (same thing as above only works with full pages cant get half)
    len = PGROUNDUP(len);

    struct proc *p = curr_proc();

    for (uint64 va = start; va < start + len; va += PGSIZE) {
        if (walkaddr(p->pagetable, va) != 0)
            return -1;
    }
	//walk pages in req range, walkaddr 0 if unmpd -1 if mpd(fail)

    //pte mpng of va to pa and perm
	int perm = PTE_U;//user
    if (port & 1) perm |= PTE_R; //read
    if (port & 2) perm |= PTE_W;//write
    if (port & 4) perm |= PTE_X;//execute

    //mapping
    for (uint64 va = start; va < start + len; va += PGSIZE) {
        void *pa = kalloc(); //kalloc allocs one pge
        if (pa == 0) {
            //cant allocated
            uvmunmap(p->pagetable, start, (va - start) / PGSIZE, 1);
            return -1;
        }
        memset(pa, 0, PGSIZE);
        if (mappages(p->pagetable, va, PGSIZE, (uint64)pa, perm) != 0) {
            kfree(pa);
            uvmunmap(p->pagetable, start, (va - start) / PGSIZE, 1);
            return -1;
        }
    }
    return 0;
}

//opposite of mmap, munmap. when we finish with a mem we free it
uint64 sys_munmap(uint64 start, uint64 len)
{
    if (len == 0) return 0;//nothing to do
    if (start % PGSIZE != 0) return -1;//start must be aligned cant do half page

    len = PGROUNDUP(len);//round up

    struct proc *p = curr_proc();

	//opposie of mmap if walkaddr rets 0 means unmapped cause err
    for (uint64 va = start; va < start + len; va += PGSIZE) {
        if (walkaddr(p->pagetable, va) == 0)
            return -1;
    }

    //unmap all
    uvmunmap(p->pagetable, start, len / PGSIZE, 1);
    return 0;
}
/*
* LAB1: you may need to define sys_task_info here
*/
uint64 sys_task_info(TaskInfo *ti)
{
	TaskInfo *physical_ti = (TaskInfo *)useraddr(curr_proc()->pagetable, (uint64)ti);
	if (physical_ti == 0)
		return -1;

	struct proc *p = curr_proc(); //this holds kernel info about proc 
	physical_ti->status = Running;
	for (int i = 0; i < MAX_SYSCALL_NUM; i++) {
		physical_ti->syscall_times[i] = p->syscall_times[i];
	}
	uint64 current_time = get_cycle() * 1000 / CPU_FREQ;
	physical_ti->time = (int)(current_time - p->start_time);
	return 0;
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
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	curr_proc()->syscall_times[id]++;
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	/*
	* LAB1: you may need to add SYS_taskinfo case here
	*/
	case SYS_task_info:
		ret = sys_task_info((TaskInfo *)args[0]);
		break;
	case SYS_mmap:
    	ret = sys_mmap(args[0], args[1], args[2], args[3], args[4]);
    	break;
	case SYS_munmap:
    	ret = sys_munmap(args[0], args[1]);
    	break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
