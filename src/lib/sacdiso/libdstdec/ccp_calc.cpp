#include "ccp_calc.h"

/* CCP = Coding of Coefficients and Ptables */

/***************************************************************************/
/*                                                                         */
/* name     : CCP_CalcInit                                                 */
/*                                                                         */
/* function : Initialise the prediction order and coefficients for         */
/*            prediction filter used to predict the filter coefficients.   */
/*                                                                         */
/* pre      : CT->TableType                                                */
/*                                                                         */
/* post     : CT->CPredOrder[], CT->CPredCoef[][]                          */
/*                                                                         */
/***************************************************************************/

__FUNCTION_ATTRIBUTES__ int CCP_CalcInit(CodedTable* CT) {
	int retval = 0;
	int i;
	switch (CT->TableType) {
	case FILTER :
		CT->CPredOrder[0]   =  1;
		CT->CPredCoef[0][0] = -8;
		for (i = CT->CPredOrder[0]; i < MAXCPREDORDER; i++) {
			CT->CPredCoef[0][i] = 0;
		}
      
		CT->CPredOrder[1]   =  2;
		CT->CPredCoef[1][0] = -16;
		CT->CPredCoef[1][1] =  8;
		for (i = CT->CPredOrder[1]; i < MAXCPREDORDER; i++) {
			CT->CPredCoef[1][i] = 0;
		}
      
		CT->CPredOrder[2]   =  3;
		CT->CPredCoef[2][0] = -9;
		CT->CPredCoef[2][1] = -5;
		CT->CPredCoef[2][2] =  6;
		for (i = CT->CPredOrder[2]; i < MAXCPREDORDER; i++) {
			CT->CPredCoef[2][i] =  0;
		}
	#if NROFFRICEMETHODS == 4
		CT->CPredOrder[3]   =  1;
		CT->CPredCoef[3][0] =  8;
		for (i = CT->CPredOrder[3]; i < MAXCPREDORDER; i++) {
			CT->CPredCoef[3][i] = 0;
		}
	#endif
		break;
	case PTABLE :
		CT->CPredOrder[0]   =  1;
		CT->CPredCoef[0][0] = -8;
		for (i = CT->CPredOrder[0]; i < MAXCPREDORDER; i++) {
			CT->CPredCoef[0][i] = 0;
		}
      
		CT->CPredOrder[1]   =  2;
		CT->CPredCoef[1][0] = -16;
		CT->CPredCoef[1][1] =  8;
		for (i = CT->CPredOrder[1]; i < MAXCPREDORDER; i++) {
			CT->CPredCoef[1][i] = 0;
		}
      
		CT->CPredOrder[2]   =  3;
		CT->CPredCoef[2][0] = -24;
		CT->CPredCoef[2][1] =  24;
		CT->CPredCoef[2][2] = -8;
		for (i = CT->CPredOrder[2]; i < MAXCPREDORDER; i++) {
			CT->CPredCoef[2][i] =  0;
		}
		break;
	default :
		trc_printf("ERROR: Illegal table type\n");
		retval = 1;
  }

  return retval;
}


