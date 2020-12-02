#ifndef __LIBS_STDARG_H__
#define __LIBS_STDARG_H__

// compiler provide size of save area

//typedef __builtin_va_list va_list;
#define va_list __builtin_va_list

#define va_start(ap, last) __builtin_va_start(ap, last)

#define va_arg(ap, type) __builtin_va_arg(ap, type)

#define va_end(ap) __builtin_va_end(ap)


#endif // __LIBS_STDARG_H__
