summary for lab3:
For merging code from lab2, I only met issue with mmap.
Issue happened because mmap memory won't be freed using previous code, which cause memory leak.
Fix for it is to record the mmap memory locations and freed them before exit.

For spawn, it's quite easy.
Only needed to create new process and set its parent to previous one.

Answers for questions:
1. Because 255 is biggest value for unsigned 8-bit integer, after this round it will be become least value and selected.

