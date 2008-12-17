#define _L4_TEST_MAIN
#include "t-environment.h"

#include <hurd/as.h>

#include "../libhurd-mm/as-compute-gbits.h"

int output_debug = 1;

void
test (void)
{
  struct as_guard_cappage gc;

  /* untranslated, to_translate, gbits.  */

  gc = as_compute_gbits_cappage (20, 10, 10);
  assert (gc.gbits == 8);
  assert (gc.cappage_width == 2);

  /* Inserting a folio at /VG_ADDR_BITS-19.  */
  gc = as_compute_gbits_cappage (30, 11, 10);
  assert (gc.gbits == 3);
  assert (gc.cappage_width == 8);

  /* Inserting a page at /VG_ADDR_BITS-12.  */
  gc = as_compute_gbits_cappage (30, 18, 10);
  assert (gc.gbits == 3);
  assert (gc.cappage_width == 8);

  /* Inserting a page at /VG_ADDR_BITS-12.  */
  gc = as_compute_gbits_cappage (30, 18, 17);
  assert (gc.gbits == 11);
  assert (gc.cappage_width == 7);

  gc = as_compute_gbits_cappage (63, 44, 30);
  assert (gc.gbits == 28);
  assert (gc.cappage_width == 8);

  gc = as_compute_gbits_cappage (4, 4, 1);
  assert (gc.gbits == 1);
  assert (gc.cappage_width == 3);
}
