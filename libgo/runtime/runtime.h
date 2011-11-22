/* runtime.h -- runtime support for Go.

   Copyright 2009 The Go Authors. All rights reserved.
   Use of this source code is governed by a BSD-style
   license that can be found in the LICENSE file.  */

#include "config.h"

#define _GNU_SOURCE
#include "go-assert.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "array.h"
#include "go-alloc.h"
#include "go-panic.h"
#include "go-string.h"

/* This file supports C files copied from the 6g runtime library.
   This is a version of the 6g runtime.h rewritten for gccgo's version
   of the code.  */

typedef signed int   int8    __attribute__ ((mode (QI)));
typedef unsigned int uint8   __attribute__ ((mode (QI)));
typedef signed int   int16   __attribute__ ((mode (HI)));
typedef unsigned int uint16  __attribute__ ((mode (HI)));
typedef signed int   int32   __attribute__ ((mode (SI)));
typedef unsigned int uint32  __attribute__ ((mode (SI)));
typedef signed int   int64   __attribute__ ((mode (DI)));
typedef unsigned int uint64  __attribute__ ((mode (DI)));
typedef float        float32 __attribute__ ((mode (SF)));
typedef double       float64 __attribute__ ((mode (DF)));
typedef unsigned int uintptr __attribute__ ((mode (pointer)));

/* Defined types.  */

typedef	uint8			bool;
typedef	uint8			byte;
typedef	struct	G		G;
typedef	union	Lock		Lock;
typedef	struct	M		M;
typedef	union	Note		Note;
typedef	struct	MCache		MCache;
typedef struct	FixAlloc	FixAlloc;

typedef	struct	__go_defer_stack	Defer;
typedef	struct	__go_panic_stack	Panic;
typedef	struct	__go_open_array		Slice;
typedef	struct	__go_string		String;

/* Per CPU declarations.  */

#ifdef __rtems__
#define __thread
#endif

extern __thread		G*	g;
extern __thread		M* 	m;

extern M	runtime_m0;
extern G	runtime_g0;

#ifdef __rtems__
#undef __thread
#endif

/* Constants.  */

enum
{
	true	= 1,
	false	= 0,
};

/*
 * structures
 */
union	Lock
{
	uint32	key;	// futex-based impl
	M*	waitm;	// linked list of waiting M's (sema-based impl)
};
union	Note
{
	uint32	key;	// futex-based impl
	M*	waitm;	// waiting M (sema-based impl)
};
struct	G
{
	Defer*	defer;
	Panic*	panic;
	void*	exception;	// current exception being thrown
	bool	is_foreign;	// whether current exception from other language
	byte*	entry;		// initial function
	G*	alllink;	// on allg
	void*	param;		// passed parameter on wakeup
	int16	status;
	int32	goid;
	int8*	waitreason;	// if status==Gwaiting
	G*	schedlink;
	bool	readyonstop;
	bool	ispanic;
	M*	m;		// for debuggers, but offset not hard-coded
	M*	lockedm;
	M*	idlem;
	// int32	sig;
	// uintptr	sigcode0;
	// uintptr	sigcode1;
	// uintptr	sigpc;
	// uintptr	gopc;	// pc of go statement that created this goroutine
};

struct	M
{
	G*	curg;		// current running goroutine
	int32	id;
	int32	mallocing;
	int32	gcing;
	int32	locks;
	int32	nomemprof;
	int32	gcing_for_prof;
	int32	holds_finlock;
	int32	gcing_for_finlock;
	int32	dying;
	int32	profilehz;
	uint32	fastrand;
	MCache	*mcache;
	M*	nextwaitm;	// next M waiting for lock
	uintptr	waitsema;	// semaphore for parking on locks
	uint32	waitsemacount;
	uint32	waitsemalock;

	/* For the list of all threads.  */
	struct __go_thread_id *list_entry;

	/* For the garbage collector.  */
	void	*gc_sp;
	size_t	gc_len;
	void	*gc_next_segment;
	void	*gc_next_sp;
	void	*gc_initial_sp;
};

/* Macros.  */

#ifdef __WINDOWS__
enum {
   Windows = 1
};
#else
enum {
   Windows = 0
};
#endif

#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))
#define	nil		((void*)0)
#define USED(v)		((void) v)

/*
 * external data
 */
extern	uint32	runtime_panicking;
int32	runtime_ncpu;

/*
 * common functions and data
 */
int32	runtime_findnull(const byte*);

/*
 * very low level c-called
 */
void	runtime_args(int32, byte**);
void	runtime_osinit();
void	runtime_goargs(void);
void	runtime_goenvs(void);
void	runtime_throw(const char*);
void*	runtime_mal(uintptr);
String	runtime_gostringnocopy(byte*);
void	runtime_mallocinit(void);
void	siginit(void);
bool	__go_sigsend(int32 sig);
int64	runtime_nanotime(void);

void	runtime_stoptheworld(void);
void	runtime_starttheworld(bool);
void	__go_go(void (*pfn)(void*), void*);
void	__go_gc_goroutine_init(void*);
void	__go_enable_gc(void);
int	__go_run_goroutine_gc(int);
void	__go_scanstacks(void (*scan)(byte *, int64));
void	__go_stealcache(void);
void	__go_cachestats(void);

/*
 * mutual exclusion locks.  in the uncontended case,
 * as fast as spin locks (just a few user-level instructions),
 * but on the contention path they sleep in the kernel.
 * a zeroed Lock is unlocked (no need to initialize each lock).
 */
void	runtime_lock(Lock*);
void	runtime_unlock(Lock*);

/*
 * sleep and wakeup on one-time events.
 * before any calls to notesleep or notewakeup,
 * must call noteclear to initialize the Note.
 * then, exactly one thread can call notesleep
 * and exactly one thread can call notewakeup (once).
 * once notewakeup has been called, the notesleep
 * will return.  future notesleep will return immediately.
 * subsequent noteclear must be called only after
 * previous notesleep has returned, e.g. it's disallowed
 * to call noteclear straight after notewakeup.
 *
 * notetsleep is like notesleep but wakes up after
 * a given number of nanoseconds even if the event
 * has not yet happened.  if a goroutine uses notetsleep to
 * wake up early, it must wait to call noteclear until it
 * can be sure that no other goroutine is calling
 * notewakeup.
 */
void	runtime_noteclear(Note*);
void	runtime_notesleep(Note*);
void	runtime_notewakeup(Note*);
void	runtime_notetsleep(Note*, int64);

/*
 * low-level synchronization for implementing the above
 */
uintptr	runtime_semacreate(void);
int32	runtime_semasleep(int64);
void	runtime_semawakeup(M*);
// or
void	runtime_futexsleep(uint32*, uint32, int64);
void	runtime_futexwakeup(uint32*, uint32);


/* Functions.  */
#define runtime_printf printf
#define runtime_malloc(s) __go_alloc(s)
#define runtime_free(p) __go_free(p)
#define runtime_memclr(buf, size) __builtin_memset((buf), 0, (size))
#define runtime_strcmp(s1, s2) __builtin_strcmp((s1), (s2))
#define runtime_mcmp(a, b, s) __builtin_memcmp((a), (b), (s))
#define runtime_memmove(a, b, s) __builtin_memmove((a), (b), (s))
#define runtime_exit(s) _exit(s)
MCache*	runtime_allocmcache(void);
void	free(void *v);
struct __go_func_type;
bool	runtime_addfinalizer(void*, void(*fn)(void*), const struct __go_func_type *);
#define runtime_mmap mmap
#define runtime_munmap(p, s) munmap((p), (s))
#define runtime_cas(pval, old, new) __sync_bool_compare_and_swap (pval, old, new)
#define runtime_casp(pval, old, new) __sync_bool_compare_and_swap (pval, old, new)
#define runtime_xadd(p, v) __sync_add_and_fetch (p, v)
#define runtime_xchg(p, v) __atomic_exchange_n (p, v, __ATOMIC_SEQ_CST)
#define runtime_atomicload(p) __atomic_load_n (p, __ATOMIC_SEQ_CST)
#define runtime_atomicstore(p, v) __atomic_store_n (p, v, __ATOMIC_SEQ_CST)
#define runtime_atomicloadp(p) __atomic_load_n (p, __ATOMIC_SEQ_CST)
#define runtime_atomicstorep(p, v) __atomic_store_n (p, v, __ATOMIC_SEQ_CST)

void	runtime_dopanic(int32) __attribute__ ((noreturn));
void	runtime_startpanic(void);
const byte*	runtime_getenv(const char*);
int32	runtime_atoi(const byte*);
void	runtime_sigprof(uint8 *pc, uint8 *sp, uint8 *lr);
void	runtime_resetcpuprofiler(int32);
void	runtime_setcpuprofilerate(void(*)(uintptr*, int32), int32);
uint32	runtime_fastrand1(void);
void	runtime_semacquire (uint32 *) asm ("libgo_runtime.runtime.Semacquire");
void	runtime_semrelease (uint32 *) asm ("libgo_runtime.runtime.Semrelease");
void	runtime_procyield(uint32);
void	runtime_osyield(void);
void	runtime_usleep(uint32);

struct __go_func_type;
void reflect_call(const struct __go_func_type *, const void *, _Bool, _Bool,
		  void **, void **)
  asm ("libgo_reflect.reflect.call");

#ifdef __rtems__
void __wrap_rtems_task_variable_add(void **);
#endif
