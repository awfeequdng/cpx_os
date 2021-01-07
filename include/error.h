#ifndef __INCLUDE_ERROR_H__
#define __INCLUDE_ERROR_H__


	// kernel error codes -- keep in sync with list in lib/printfmt.c
#define		E_UNSPECIFIED		1
#define		E_BAD_PROCESS		2
#define 	E_INVAL				3	// invalid parameter
#define 	E_NO_MEM			4
#define		E_NO_FREE_PROCESS	5
#define 	E_FAULT				6  // memory fault
#define 	E_SWAP_FAULT		7  // swap read/write fault 


#define 	MAX_ERROR			7


#endif // __INCLUDE_ERROR_H__
