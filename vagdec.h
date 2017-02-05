#pragma once

#include "mss/mss.h"

enum PROPERTY
{
   //
   // Additional decoder props (beyond standard RIB PROVIDER_ properties)
   //

   IN_FTYPES,               // STRING supported input file types
   IN_WTAG,                 // Tag used for this data in a wave file
   OUT_FTYPES,              // STRING supported output file types
   FRAME_SIZE,              // S32 worst-case frame buffer size

   INPUT_BIT_RATE,          // S32 input bit rate
   INPUT_SAMPLE_RATE,       // S32 sample rate of input data
   INPUT_BITS,              // S32 bit width per sample of input data
   INPUT_CHANNELS,          // S32 # of channels in input data

   OUTPUT_BIT_RATE,         // S32 output bit rate
   OUTPUT_SAMPLE_RATE,      // S32 output sample rate
   OUTPUT_BITS,             // S32 bit width per sample of output data
   OUTPUT_CHANNELS,         // S32 # of channels in output data

   POSITION,                // S32 bytes processed so far
   PERCENT_DONE,            // % percent done
   MIN_INPUT_BLOCK_SIZE,    // S32 minimum block size for input

   //
   // Codec-specific stream props
   //
   //INTERLEAVE,

   //
   // Stream properties
   //

   //RAW_RATE,
   //RAW_BITS,
   //RAW_CHANNELS,

   REQUESTED_RATE,          // S32 requested rate for output data
   REQUESTED_BITS,          // S32 requested bit width for output data
   REQUESTED_CHANS          // S32 requested # of channels for output data
};

struct VAG_HEADER // Big endian
{
	char VAG[3];
	char field_3;       // can be p or i
	int  version;       // doesn't seem to matter at all
	int  field_8;       // always 0?
	int  size;          // size minus 0x30
	int  sample_rate;
	int  stereo;
	int  field_18[2];   // always 0?
	char filename[16];
	char padding[16];
};

struct ASISTREAM
{
	U32 user;
	U32 size;
	AILASIFETCHCB fetch_CB;

	U32 offset;

	S32 cursor;
	U32 blocks;
	U32 cur_block;

	S32 num_of_channels;

	bool loop;

	VAG_HEADER VAGheader;

	struct
	{
		double s_1, s_2;
		double samples[28];
		S16 decoded_samples[28];
	} channels[2];
};

void RegisterVAGInterface();
void UnRegisterVAGInterface();

U32 AILCALL FAR PROVIDER_query_attribute(HATTRIB index);
ASIRESULT AILCALL FAR ASI_startup(void);
ASIRESULT AILCALL FAR ASI_shutdown(void);
C8 FAR* AILCALL FAR ASI_error(void);

HASISTREAM AILCALL FAR ASI_stream_open(U32 user, AILASIFETCHCB fetch_CB, U32 total_size);
ASIRESULT AILCALL ASI_stream_close(HASISTREAM stream);
ASIRESULT AILCALL ASI_stream_seek (HASISTREAM stream, S32 stream_offset);
S32 AILCALL FAR ASI_stream_process(HASISTREAM stream, void FAR *buffer, S32 buffer_size);
S32 AILCALL FAR ASI_stream_attribute (HASISTREAM stream, HATTRIB attrib);
S32 AILCALL FAR ASI_stream_set_preference (HASISTREAM stream, HATTRIB preference, void FAR* value);