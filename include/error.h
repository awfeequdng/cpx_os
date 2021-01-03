#ifndef __INCLUDE_ERROR_H__
#define __INCLUDE_ERROR_H__

enum Error {
	// kernel error codes -- keep in sync with list in lib/printfmt.c
	E_INVAL	,	// invalid parameter
	E_NO_MEM,
	E_SWAP_FAULT,
	
	MAX_ERROR
};

#endif // __INCLUDE_ERROR_H__
