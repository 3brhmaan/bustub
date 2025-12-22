#### **Project #1 - Buffer Pool Manager**
Implement Project #1 - Buffer Pool Manager with LRU-K, Disk Scheduler, and Page Guards
- **LRU-K Replacement Policy**: Built a thread-safe eviction mechanism that tracks page access history using backward k-distance to intelligently select victims when memory is full, maintaining separate tracking for frames with fewer than k accesses.
- **Disk Scheduler**: Designed a concurrent request queue with background worker thread to asynchronously process disk I/O operations (reads/writes), providing non-blocking I/O through promise/future synchronization.
- **Buffer Pool Manager**: Integrated the LRU-K replacer and disk scheduler to manage in-memory page frames, handle page pinning/unpinning with atomic reference counts, and maintain a page-to-frame mapping table for efficient lookups.
- **RAII Page Guards**: Implemented ReadPageGuard and WritePageGuard with move semantics to provide thread-safe, automatically-released access to page data, preventing memory leaks and ensuring proper latch release.
- **Concurrency & Correctness**: Employed fine-grained latching strategies and atomic operations throughout to prevent data races, ensure single-page-in-memory invariant, and handle multiple concurrent threads safely.

![alt text](./images/project%201.png)