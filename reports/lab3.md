summary for lab3:
For merging code from lab2, I only met issue with mmap.
Issue happened because mmap memory won't be freed using previous code, which cause memory leak.
Fix for it is to record the mmap memory locations and freed them before exit.

For spawn, it's quite easy.
Only needed to create new process and set its parent to previous one.

For stride scheduling algorithm, it's quite easy since all the preconditions are explained in advance.
We need to set proper stride and priority during initialization.
Also we need to implement sys_set_priority which is quite straightforward.
Then each time when add_task, we need to select one task which has smallest stride. I still reuses the queue structure for minimizing code change, but it's ugly and not easy to understand. 

Answers for questions:
1. Because 255 is biggest value for unsigned 8-bit integer, after this round it will be become least value and selected.

