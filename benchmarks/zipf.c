#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#include "zipf.h"

/* Based on Luc Devroye's rejection method for sampling from the Zipf
   distribution (Non-Uniform Random Variate Generation, page 550,
   http://cg.scs.carleton.ca/~luc/chapter_ten.pdf).  The code is based
   on that written by Michael Dürr:
   http://lists.gnu.org/archive/html/help-gsl/2008-05/msg00057.html
   .  */
int
rand_zipf (double skew, int limit)
{
  double a = skew;
  double b = pow (2.0, a - 1.0);
  double X, T, U, V;
  do
    {
      U = drand48 ();
      V = drand48 ();
      X = floor (pow (U, -1.0 / (a - 1.0)));
      T = pow (1.0 + 1.0 / X, a - 1.0);
    }
  while ((V * X * (T - 1.0) / (b - 1.0) > (T / b))
	 /* If the number is larger than the user requested limmit, we
	    just try again.  This does not effect the
	    distribution.  */
	 || X > (double) INT32_MAX
	 || (int) X > limit);
  return (int) X;
}

#if 0
#include <stdio.h>

/* To see the resulting distribution, use gnuplot and run:

     plot "<./zipf | sort -n | uniq -c" using 2:1
 */
int
main (int argc, char *argv[])
{
  double skew = 1.4;
  if (argc > 1)
    {
      skew = atof (argv[1]);
      if (skew <= 1.0)
	{
	  fprintf (stderr, "skew must be >= 1, you said %f\n", skew);
	  exit (1);
	}
    }

  int i;
  for (i = 0; i < 100000; i ++)
    printf ("%u\n", rand_zipf (skew, 10000));

  return 0;
}
#endif

