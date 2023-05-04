Summary for lab1:
  This lab is mainly used to implement one system call sys_task_info(TaskInfo *ti);
  It's quite straightforward to implement needed function.
  The very first step is to define struct TaskInfo and add member to struct proc.
  To get the total time of each running process, we also need to track time when each 
  process is starting to be scheduled.
  Moreover, the track time for first scheduling should only happen for the first time 
  when it's scheduled.
  So I added following 3 members to struct proc:
  TaskInfo taskInfo;
  int starttime;
  int firstaccess;
  After defining those members, we need to initialize all the members when all processes 
  are being initlized.
  When each process is first scheduled, we need to get starttime using get_cycle.
  We also needed to update the task status when process is loaded and scheduled.
  Then we implement sys_task_info to get all the needed information.
  But when I started to execute test program, I found that ch3_taskinfo is never executed.
  I need to manually added it to test Makefile.
  Also each time I modified test makefile or test program, I had to "make clean" firstly
  to make sure all my changes are really included.

Questions:
1.  when I'm trying to dereferernce a pointer which is pointing to zero, I got following error and exited:
    [ERROR 1]unknown trap: 0x0000000000000007, stval = 0x0000000000000000
    My RustSbi version is " RustSBI version 0.3.0-alpha.2".
2.  
  1) a0 means TRAMPFRAME. a1 means page table. 
  2) sfence.vma is used to invalidate local TLB after modifying page table. 
     Deleting this instruction won't cause any issue.
  3) a0 is already stored under sscratch and a0 is used to store TRAMPFRAME.
     a0 represents TRAMPFRAME.
     a0 is stored under sscratch.
  4) sret would cause status change. It's used to return from supervisor mode to user mode.
  5) After L29, a0 stores TRAPFRAME, sscratch stores user page table.
  6) It stores element from 6th one(ra).
     It store every element from the 6th one.
  7) After "jr t0", it enters S mode. 
  8) t0 stores kernel_trap which means usertrap function.
     kernel_trap is set under usertrapret function. 

