#include "dst_fram.h"
#include "dst_init.h"
#include "dst_decoder.h"

__FUNCTION_ATTRIBUTES__ int Init(DstDec* D, int NrChannels, int Fs44) {
	D->FrameHdr.NrOfChannels   = NrChannels;
	D->FrameHdr.FrameNr        = 0;
	D->StrFilter.TableType     = FILTER;
	D->StrPtable.TableType     = PTABLE;
	/*  64FS =>  4704 */
	/* 128FS =>  9408 */
	/* 256FS => 18816 */
	D->FrameHdr.MaxFrameLen    = (588 * Fs44 / 8); 
	D->FrameHdr.ByteStreamLen  = D->FrameHdr.MaxFrameLen   * D->FrameHdr.NrOfChannels;
	D->FrameHdr.BitStreamLen   = D->FrameHdr.ByteStreamLen * RESOL;
	D->FrameHdr.NrOfBitsPerCh  = D->FrameHdr.MaxFrameLen   * RESOL;
	D->FrameHdr.MaxNrOfFilters = 2 * D->FrameHdr.NrOfChannels;
	D->FrameHdr.MaxNrOfPtables = 2 * D->FrameHdr.NrOfChannels;
	return DST_InitDecoder(D);
}

__FUNCTION_ATTRIBUTES__ int Close(DstDec* D) {
	return 0;
}
 
__FUNCTION_ATTRIBUTES__ int Decode(DstDec* D, uint8_t* DSTFrame, uint8_t* DSDMuxedChannelData, int FrameCnt, int* FrameSize) {
	return DST_FramDSTDecode(D, DSTFrame, DSDMuxedChannelData, *FrameSize, FrameCnt);
}
