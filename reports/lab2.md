summary for lab2:
  This lab is not very complicated since mainly it's only needed to use kernel APIs to implement syscall.
  For sys_gettimeofday and taskinfo, it's very easy since it's almost same as previous lab, the only code needed to be added is to use copyout to copy kernel variables to user space.
  For mmap and munmap, there's pretty good guide on all exceptional conditions which need to be cared about, it's quite handy to convert them to c code. I also need to be careful that length needs to be rounded up to whole page.Under mmap I need to check exceptional condition and allocate each page and map them accordingly. For munmap, I need to unmap each page with free operation.

Answer to questions:
1. Page table entry including following bits:
   63-54: reserved
   53-28: PPN[2](Phisical page number)
   27-19: PPN[1]
   18-10: PPN[0]
   7-0: D A G U X W R V
   U(user),X(executable),W(write),R(read),V(valid)
2. 1) load page fault and store page fault could cause page exception.
      Important registers when exception happened are sstatus,epc, satp,sscratach 
   2) Lazy policy makes coding easier.
      COW for 10G memory need 256MBytes.
      For lazy policy, we don't need to allocate all the memory when loading the program.
      We only need to allocate page when code is executed or data is accessed.
   3) Invalid page could happen that V is 0 for PTE.
3. 1) For one page table, we don't need to switch table
   2) There's U bit in PTE which means user mode access.
   3) Easier to operate and maintaining code.
   4) For two page table switch, we need to change page table when system call,interrupt and exception happened and also when after that we return from kernel to user mode.
     If only one page table is used. No need to switch table.
     
