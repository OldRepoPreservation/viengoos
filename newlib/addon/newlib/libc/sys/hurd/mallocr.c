#include <stdlib.h>

void *
_malloc_r (struct _reent *reent_ptr, size_t s)
{
  return malloc (s);
}
void
_free_r (struct _reent *reent_ptr, void *p)
{
  free (p);
}

void *
_realloc_r (struct _reent *reent_ptr, void *p, size_t s)
{
  return realloc (p, s);
}

void *
_calloc_r (struct _reent *reent_ptr, size_t n, size_t s)
{
  return calloc (n, s);
}

void
_cfree_r (struct _reent *reent_ptr, void *p)
{
  free (p);
}
