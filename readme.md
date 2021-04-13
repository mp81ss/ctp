# CTP
## The universal C-Thread-Pool library

*CTP* is a C library that offers a pool thread, based on _pthread_, that is the only dependency.\
While easy to use, it has some interesting features:

### Features
- Cross platform: Standard C; Windows, linux, bsd; 32/64 bit
- Can spawn the best number of threads according to detected cpu
- Lazy thread activation
- Ability to pause/resume
- Automatic/custom queue size
- Can block when adding work or discard if queue is full (best effort)
- Possibility to know how many threads were effectively spawned
- Dedicated API to query status in any moment (paused/idle/working)
- Easy transition from _pthread_, the work prototype has the same signature

### Installation
Just compile the .c file and add it to your linker, as object or library.
If using gcc/clang, remember to complie with _-pthread_ and link with _-lpthread_

If you want to use visual studio, or other windows compiler, use package
<https://sourceware.org/pthreads-win32>

### Configuration
While compiling ctp (The library, not the clients), you can pass these variables:
- CTP_DEFAULT_THREADS_NUM
- CTP_MULTIPLY_QUEUE_FACTOR
- CTP_MIN_QUEUE_SIZE

_CTP_DEFAULT_THREADS_NUM_ is used only if you pass 0 to init, and _ctp_ fails to detect core number.\
In this case, _CTP_DEFAULT_THREADS_NUM_ threads will be used. Default is **4**.\
If you pass 0 to init in the second parameter, the queue size will be calculated according to 3 parameters:\
_threads-num_, _CTP_MULTIPLY_QUEUE_FACTOR_ and _CTP_MIN_QUEUE_SIZE_. The formula is:\
```max(threads-num * CTP_MULTIPLY_QUEUE_FACTOR, CTP_MIN_QUEUE_SIZE)```\
The default value for _CTP_MULTIPLY_QUEUE_FACTOR_ is **8**.\
The default value for _CTP_MIN_QUEUE_SIZE_ is **256**

---

For API details see header file. Test file may help too
