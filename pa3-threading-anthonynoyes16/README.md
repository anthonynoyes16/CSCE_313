Instruction: https://docs.google.com/document/d/1aA-ieWpgfp6r_iODC2Y1gs_6UlWE8Pxu7L0Bld1_Vu0

Data transfers:
    - create p threads to produce datamsgs (w/ ecg1)
    - create w FIFOs for workers to use
    - create w threads to consume and process datamsgs
        - worker threads produce result of process datamsgs
    - create h threads to consume results and populate HG (w/ update(...) function)

file transfers:
    - collect file size
    - create a thread to produce filemsgs
    - create w-threads to consume and process filemsgs
        - use fseek (w/ SEEK_SET) to write to file (open mode is important)

Bounded buffer:
    - STL queue with vector<char>
    - use a mutex and a condition variable
        - mutex is wrapped in a unique_lock
    - push waits on size < cap, notify pop that data is available (w/ condition variable)
    - pop waits on size > 0, notify push that slot in buffer is available (w/ condition variable)

Client:
    - need to join() threads after making them
