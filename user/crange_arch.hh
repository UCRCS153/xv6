#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <atomic>
#include <utility>

extern "C" {
#include <string.h>
}

typedef uint32_t  u32;
typedef uint64_t  u64;
typedef uintptr_t uptr;

#define cprintf(...) printf(__VA_ARGS__)
#define panic(...) do { printf(__VA_ARGS__); assert(0); } while (0)
#define LOCKSTAT_CRANGE 0
#define LOCKSTAT_GC     0

struct spinlock {
  pthread_mutex_t mu;
};

struct condvar {
  pthread_cond_t cv;
};

static inline void
acquire(spinlock *s)
{
  pthread_mutex_lock(&s->mu);
}

static inline void
release(spinlock *s)
{
  pthread_mutex_unlock(&s->mu);
}

static inline int
tryacquire(spinlock *s)
{
  return !pthread_mutex_trylock(&s->mu);
}

static inline void
initlock(spinlock *s, const char *m, int lockstat)
{
  memset(s, 0, sizeof(*s));
}

static inline void
cv_wakeup(condvar *c)
{
  pthread_cond_signal(&c->cv);
}

static inline void
cv_sleepto(condvar *c, spinlock *s, u64 ns)
{
  timespec ts;
  ts.tv_sec  = ns / 1000000000;
  ts.tv_nsec = ns % 1000000000;
  pthread_cond_timedwait(&c->cv, &s->mu, &ts);
}

static inline u64
nsectime()
{
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (((u64) 1000000000) * ts.tv_sec) + ts.tv_nsec;
}

static inline void
initcondvar(condvar *c, const char *m)
{
  memset(c, 0, sizeof(*c));
}

static inline int
kmalign(void **ptr, size_t align, size_t size)
{
  return posix_memalign(ptr, align, size);
}

static inline void
kmalignfree(void *ptr)
{
  free(ptr);
}

struct proc {
  spinlock gc_epoch_lock;
  u64 epoch;
  u64 epoch_depth;
  u32 cpuid;
  u32 pid;
  char name[32];
};

struct cpu {
  u32 id;
  u32 ncli;
};

extern pthread_key_t myproc_key;
extern cpu cpus[];
extern proc procs[];
extern u32 ncpu;
extern u64 ticks;

static inline proc*
myproc()
{
  return (proc*) pthread_getspecific(myproc_key);
}

static inline cpu*
mycpu()
{
  return (cpu*) &cpus[myproc()->cpuid];
}

void cli();
void sti();

static inline void
pushcli()
{
  cli();
  mycpu()->ncli++;
}

static inline void
popcli()
{
  if (--mycpu()->ncli == 0)
    sti();
}

void threadpin(void (*fn)(void*), void *arg, const char *name, int cpu);

