/*

MPEG-4 Audio RM Module
Lossless coding of 1-bit oversampled audio - DST (Direct Stream Transfer)

This software was originally developed by:

* Aad Rijnberg
	Philips Digital Systems Laboratories Eindhoven
	<aad.rijnberg@philips.com>

* Fons Bruekers
	Philips Research Laboratories Eindhoven
	<fons.bruekers@philips.com>

* Eric Knapen
	Philips Digital Systems Laboratories Eindhoven
	<h.w.m.knapen@philips.com>

And edited by:

* Richard Theelen
	Philips Digital Systems Laboratories Eindhoven
	<r.h.m.theelen@philips.com>

* Maxim V.Anisiutkin
	<maxim.anisiutkin@gmail.com>

in the course of development of the MPEG-4 Audio standard ISO-14496-1, 2 and 3.
This software module is an implementation of a part of one or more MPEG-4 Audio
tools as specified by the MPEG-4 Audio standard. ISO/IEC gives users of the
MPEG-4 Audio standards free licence to this software module or modifications
thereof for use in hardware or software products claiming conformance to the
MPEG-4 Audio standards. Those intending to use this software module in hardware
or software products are advised that this use may infringe existing patents.
The original developers of this software of this module and their company,
the subsequent editors and their companies, and ISO/EIC have no liability for
use of this software module or modifications thereof in an implementation.
Copyright is not released for non MPEG-4 Audio conforming products. The
original developer retains full right to use this code for his/her own purpose,
assign or donate the code to a third party and to inhibit third party from
using the code for non MPEG-4 Audio conforming products. This copyright notice
must be included in all copies of derivative works.

Copyright © 2004.

*/

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


