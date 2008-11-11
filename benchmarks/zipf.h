#ifndef ZIPF_H
#define ZIPF_H

/* Return a random number from a zipf distribution between 1 and limit
   inclusive.  Alpha must be > 1.  */
extern int rand_zipf (double alpha, int limit);

#endif
