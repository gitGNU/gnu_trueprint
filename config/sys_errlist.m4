AC_DEFUN(TRUEPRINT_VAR_SYS_ERRLIST,
[AC_CACHE_CHECK([for sys_errlist],
trueprint_cv_var_sys_errlist,
[AC_TRY_LINK([int *p;], [extern in sys_errlist; p = &sys_errlist;],
	trueprint_cv_var_sys_errlist=yes, trueprint_cv_var_sys_errlist=no)])
if test x"$trueprint_cv_var_sys_errlist" = xyes; then
  AC_DEFINE(HAVE_SYS_ERRLIST, 1,
    [Define if your system libraries have a sys_errlist variable.])
fi])
