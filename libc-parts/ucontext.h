#ifndef _UCONTEXT_H
#define _UCONTEXT_H 1

#include <signal.h>

typedef struct
{
} mcontext_t;

struct ucontext;

typedef struct
{
  /* Pointer to the context that is resumed when this context
     returns. */
  struct ucontext *uc_link;
  /* The set of signals that are blocked when this context is
     active.  */
  sigset_t uc_sigmask;

  /* The stack used by this context.  */
  stack_t uc_stack;

  /* A machine-specific representation of the saved context.  */
  mcontext_t uc_mcontext;
} ucontext_t;

int getcontext(ucontext_t *);
void makecontext(ucontext_t *, void (*)(void), int, ...);
int setcontext(const ucontext_t *);
int swapcontext(ucontext_t *restrict, const ucontext_t *restrict);

#endif
