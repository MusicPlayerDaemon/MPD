#include "ccp_calc.h"
#include "dst_init.h"

__FUNCTION_ATTRIBUTES__ int DST_InitDecoder(DstDec* D) {
  int retval = 0;
  D->FrameHdr.FrameNr = 0;
  D->StrFilter.TableType = FILTER;
  D->StrPtable.TableType = PTABLE;
  if (retval == 0) {
		retval = CCP_CalcInit((CodedTable*)&D->StrFilter);
  }
	if (retval == 0) {
		retval = CCP_CalcInit((CodedTable*)&D->StrPtable);
  }
  return retval;
}
