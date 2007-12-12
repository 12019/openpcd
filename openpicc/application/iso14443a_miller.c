/* ISO14443A Manchester encoder for OpenPICC
 * (C) 2007 by Henryk Plötz <henryk@ploetzli.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <openpicc.h>
#include <FreeRTOS.h>

#include <string.h>

#include "iso14443_layer3a.h"
#include "usb_print.h"
#include "cmd.h"

#ifdef FOUR_TIMES_OVERSAMPLING
#define OVERSAMPLING_RATE	4

/* definitions for four-times oversampling */
#define SEQ_X	0x4
#define SEQ_Y	0x0
#define SEQ_Z	0x1
#else
#define OVERSAMPLING_RATE	2
#define SEQ_X   0x2
#define SEQ_Y   0x0
#define SEQ_Z   0x1
#endif

enum miller_sequence {
	SEQUENCE_X,
	SEQUENCE_Y,
	SEQUENCE_Z,
};

#define BIT_ENDMARKER -1

int iso14443a_decode_miller(iso14443_frame *frame, 
	const u_int8_t *sample_buf, const u_int16_t sample_buf_len)
{
	signed int i, j, bit = 0, last_bit = 0;
	enum miller_sequence current_seq;
	unsigned int bitpos = 0;
	
	memset(frame, 0, sizeof(frame));
	frame->type = TYPE_A;
	frame->parameters.a.parity = GIVEN_PARITY;
	
	for(i=0; i<sample_buf_len && bit != BIT_ENDMARKER; i++) {
		for(j=0; j<(signed)(sizeof(sample_buf[0])*8)/OVERSAMPLING_RATE && bit != BIT_ENDMARKER; j++) {
			int sample = (sample_buf[i]>>(j*OVERSAMPLING_RATE)) & ~(~0 << OVERSAMPLING_RATE);
			switch(sample) {
				case SEQ_X: current_seq = SEQUENCE_X; break;
				case SEQ_Y: current_seq = SEQUENCE_Y; break;
				case SEQ_Z: current_seq = SEQUENCE_Z; break;
				default: current_seq = SEQUENCE_Y;
			}
			
			switch(current_seq) {
				case SEQ_X:
				DumpStringToUSB("X");
					bit = 1; break;
				case SEQ_Y: /* Fall-through to SEQ_Z */
				DumpStringToUSB("Y");
					if(last_bit == 0) {
						bit = BIT_ENDMARKER;
						break;
					}
				case SEQ_Z:
				DumpStringToUSB("Z");
					bit = 0; break;
			}
			
			switch(bit) {
				case BIT_ENDMARKER:
					bitpos--;
					break;
				case 0: /* Fall-through */
				case 1: {
					int bytepos = bitpos/9;
					if(bitpos % 9 == 8) { /* Parity bit */
						frame->parity[ bytepos/8 ] |= (bit<<(bytepos%8));
					} else {
						frame->data[ bytepos ] |= (bit<<(bitpos%9));
					}
				}
			}
			
			last_bit = bit;
			bitpos++;
		}
	}
	
	frame->numbytes = bitpos/9;
	frame->numbits = bitpos%9;
	DumpStringToUSB("\n\r");
	
	return 0;
}
