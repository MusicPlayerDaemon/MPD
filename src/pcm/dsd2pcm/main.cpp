/*

Copyright 2009, 2011 Sebastian Gesemann. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY SEBASTIAN GESEMANN ''AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEBASTIAN GESEMANN OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Sebastian Gesemann.

 */

#include <iostream>
#include <vector>
#include <cstring>

#include "dsd2pcm.hpp"
#include "noiseshape.hpp"

namespace {

const float my_ns_coeffs[] = {
//     b1           b2           a1           a2
  -1.62666423,  0.79410094,  0.61367127,  0.23311013,  // section 1
  -1.44870017,  0.54196219,  0.03373857,  0.70316556   // section 2
};

const int my_ns_soscount = sizeof(my_ns_coeffs)/(sizeof(my_ns_coeffs[0])*4);

inline long myround(float x)
{
	return static_cast<long>(x + (x>=0 ? 0.5f : -0.5f));
}

template<typename T>
struct id { typedef T type; };

template<typename T>
inline T clip(
	typename id<T>::type min,
	T v,
	typename id<T>::type max)
{
	if (v<min) return min;
	if (v>max) return max;
	return v;
}

inline void write_intel16(unsigned char * ptr, unsigned word)
{
	ptr[0] =  word       & 0xFF;
	ptr[1] = (word >> 8) & 0xFF;
}

inline void write_intel24(unsigned char * ptr, unsigned long word)
{
	ptr[0] =  word        & 0xFF;
	ptr[1] = (word >>  8) & 0xFF;
	ptr[2] = (word >> 16) & 0xFF;
}

} // anonymous namespace

using std::vector;
using std::cin;
using std::cout;
using std::cerr;

int main(int argc, char *argv[])
{
	const int block = 16384;
	int channels   = -1;
	int lsbitfirst = -1;
	int bits       = -1;
	if (argc==4) {
		if ('1'<=argv[1][0] && argv[1][0]<='9') channels = 1 + (argv[1][0]-'1');
		if (argv[2][0]=='m' || argv[2][0]=='M') lsbitfirst=0;
		if (argv[2][0]=='l' || argv[2][0]=='L') lsbitfirst=1;
		if (!strcmp(argv[3],"16")) bits = 16;
		if (!strcmp(argv[3],"24")) bits = 24;
	}
	if (channels<1 || lsbitfirst<0 || bits<0) {
		cerr << "\n"
			"DSD2PCM filter (raw DSD64 --> 352 kHz raw PCM)\n"
			"(c) 2009 Sebastian Gesemann\n\n"
			"(filter as in \"reads data from stdin and writes to stdout\")\n\n"
			"Syntax: dsd2pcm <channels> <bitorder> <bitdepth>\n"
			"channels = 1,2,3,...,9 (number of channels in DSD stream)\n"
			"bitorder = L (lsb first), M (msb first) (DSD stream option)\n"
			"bitdepth = 16 or 24 (intel byte order, output option)\n\n"
			"Note: At 16 bits/sample a noise shaper kicks in that can preserve\n"
			"a dynamic range of 135 dB below 30 kHz.\n\n";
		return 1;
	}
	int bytespersample = bits/8;
	vector<dxd> dxds (channels);
	vector<noise_shaper> ns;
	if (bits==16) {
		ns.resize(channels, noise_shaper(my_ns_soscount, my_ns_coeffs) );
	}
	vector<unsigned char> dsd_data (block * channels);
	vector<float> float_data (block);
	vector<unsigned char> pcm_data (block * channels * bytespersample);
	char * const dsd_in  = reinterpret_cast<char*>(&dsd_data[0]);
	char * const pcm_out = reinterpret_cast<char*>(&pcm_data[0]);
	while (cin.read(dsd_in,block * channels)) {
		for (int c=0; c<channels; ++c) {
			dxds[c].translate(block,&dsd_data[0]+c,channels,
				lsbitfirst,
				&float_data[0],1);
			unsigned char * out = &pcm_data[0] + c*bytespersample;
			if (bits==16) {
				for (int s=0; s<block; ++s) {
					float r = float_data[s]*32768 + ns[c].get();
					long smp = clip(-32768,myround(r),32767);
					ns[c].update( clip(-1,smp-r,1) );
					write_intel16(out,smp);
					out += channels*bytespersample;
				}
			} else {
				for (int s=0; s<block; ++s) {
					float r = float_data[s]*8388608;
					long smp = clip(-8388608,myround(r),8388607);
					write_intel24(out,smp);
					out += channels*bytespersample;
				}
			}
		}
		cout.write(pcm_out,block*channels*bytespersample);
	}
}

