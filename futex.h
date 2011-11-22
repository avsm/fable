#ifndef FUTEX_H__
#define FUTEX_H__

static inline unsigned
atomic_cmpxchg(volatile unsigned *loc, unsigned old, unsigned new)
{
  unsigned long res;
  asm ("lock cmpxchg %3, %1\n"
       : "=a" (res), "=m" (*loc)
       : "0" (old),
	 "r" (new),
	 "m" (*loc)
       : "memory");
  return res;
}

static inline unsigned
atomic_xchg(volatile unsigned *loc, unsigned new)
{
  unsigned long res;
  asm ("xchg %0, %1\n"
       : "=r" (res),
	 "=m" (*loc)
       : "m" (*loc),
	 "0" (new)
       : "memory");
  return res;
}

static inline int
futex(volatile unsigned *slot, int cmd, unsigned val, const struct timespec *ts,
      int *uaddr, int val2)
{
  return syscall(SYS_futex, slot, cmd, val, ts, uaddr, val2);
}

static inline void
futex_wait_while_equal(volatile unsigned *slot, unsigned val)
{
  assert((unsigned long)slot % 4 == 0);
  if (futex(slot, FUTEX_WAIT, val, NULL, NULL, 0) < 0 && errno != EAGAIN)
    err(1, "futex_wait");
}

static inline void
futex_wake(volatile unsigned *slot)
{
  if (futex(slot, FUTEX_WAKE, 1, NULL, NULL, 0) < 0)
    err(1, "futex_wake");
}

#endif /* !FUTEX_H__ */
