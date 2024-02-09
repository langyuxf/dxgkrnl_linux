
# reproduce steps
1, submit copy commands which copy 0xdeadbeefdeadbeef from src_gpu_va to dst_gpu_va  
2, fork a child process  
3, trigger copy-on-write from parent process on dst_gpu_va page  

# get_user_pages_fast
child: tgid = 26, pid = 26, dst_cpu = deadbeefdeadbeef  
parent: tgid = 25, pid = 25, dst_cpu = 0000000000000000  

# pin_user_pages_fast 
child: tgid = 8129, pid = 8129, dst_cpu = 0000000000000000  
parent: tgid = 8128, pid = 8128, dst_cpu = deadbeefdeadbeef  

# note
Please use AMD gfx9 GPU and above with Hardware Accelerated GPU Scheduling disabled.