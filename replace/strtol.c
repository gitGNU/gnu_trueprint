/*
 * Stub for strtol - only works with base 10
 */
long int strtol(const char *nptr, char **endptr, int base)
{
  long int retval = 0;
  char *cptr;

  if (base != 10)
    abort();

  *endptr = nptr;

  while (('0' <= **endptr) && (**endptr <= '9'))
    {
      retval = retval * 10 + (**endptr - '0');
      (*endptr)++;
    }

  return retval;
}

