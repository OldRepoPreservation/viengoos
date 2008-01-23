int
isatty (int fd)
{
  /* XXX: Assume the FD is valid.  The only valid FDs are for the
     console, which is a terminal.  */
  return 1;
}
