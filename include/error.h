#ifndef __INCLUDE_ERROR_H__
#define __INCLUDE_ERROR_H__

enum {
	// kernel error codes -- keep in sync with list in lib/printfmt.c
	E_INVAL	,	// invalid parameter
	E_NO_MEM,
	
	MAX_ERROR
};

#endif // __INCLUDE_ERROR_H__
