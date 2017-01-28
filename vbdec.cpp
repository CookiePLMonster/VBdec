#include "vbdec.h"

#define GET_BLOCK(offset) ( offset >> 13 )


static HPROVIDER providerHandle;
void RegisterVBInterface()
{
	// ASI codec
	const RIB_INTERFACE_ENTRY codecEntries[] = {
		REG_FN(PROVIDER_query_attribute),
		REG_AT("Name", PROVIDER_NAME, RIB_STRING),
		REG_AT("Version", PROVIDER_VERSION, RIB_HEX),
		REG_AT("Input file types", IN_FTYPES, RIB_STRING),
		//REG_AT("Input wave tag", IN_WTAG, RIB_DEC),
		REG_AT("Output file types", OUT_FTYPES, RIB_STRING),
		REG_AT("Maximum frame size", FRAME_SIZE, RIB_DEC),
		REG_FN(ASI_startup),
		REG_FN(ASI_error),
		REG_FN(ASI_shutdown),
	};

	// ASI stream
	const RIB_INTERFACE_ENTRY streamEntries[] = { 
		REG_FN( ASI_stream_open ),
		REG_FN( ASI_stream_process ),
		REG_FN( ASI_stream_attribute ),
		REG_FN( ASI_stream_set_preference ),
		REG_FN( ASI_stream_seek ),
		REG_FN( ASI_stream_close ),
		REG_AT("Input bit rate",           INPUT_BIT_RATE,       RIB_DEC),
		REG_AT("Input sample rate",        INPUT_SAMPLE_RATE,    RIB_DEC),
		REG_AT("Input sample width",       INPUT_BITS,           RIB_DEC),
		REG_AT("Input channels",           INPUT_CHANNELS,       RIB_DEC),
		REG_AT("Output bit rate",          OUTPUT_BIT_RATE,      RIB_DEC),
		REG_AT("Output sample rate",       OUTPUT_SAMPLE_RATE,   RIB_DEC),
		REG_AT("Output sample width",      OUTPUT_BITS,          RIB_DEC),
		REG_AT("Output channels",          OUTPUT_CHANNELS,      RIB_DEC),
		REG_AT("Position",                 POSITION,             RIB_DEC),
		REG_AT("Percent done",             PERCENT_DONE,         RIB_PERCENT),
		REG_AT("Minimum input block size", MIN_INPUT_BLOCK_SIZE, RIB_DEC),
		//REG_AT("MPEG version",             MPEG_VERSION,         RIB_DEC),
		//REG_AT("MPEG layer",               MPEG_LAYER,           RIB_DEC),
		//REG_AT("Interleave",               INTERLEAVE,           RIB_DEC),
		//REG_PR("Raw source sample rate",   RAW_RATE,             RIB_DEC),
		//REG_PR("Raw source sample width",  RAW_BITS,             RIB_DEC),
		//REG_PR("Raw source channels",      RAW_CHANNELS,         RIB_DEC),
		REG_PR("Requested sample rate",    REQUESTED_RATE,       RIB_DEC),
		REG_PR("Requested sample width",   REQUESTED_BITS,       RIB_DEC),
		REG_PR("Requested channels",       REQUESTED_CHANS,      RIB_DEC)
	};

	providerHandle = RIB_provider_library_handle();
	RIB_register( providerHandle, "ASI codec", codecEntries );
	RIB_register( providerHandle, "ASI stream", streamEntries );
}

void UnregisterVBInterface()
{
	RIB_unregister_all(providerHandle);
}

// ASI codec

U32 AILCALL FAR PROVIDER_query_attribute(HATTRIB index)
{
	switch ( index )
	{
	case PROVIDER_NAME:
		return (U32)"MSS VB Audio Decoder";
	case PROVIDER_VERSION:
		return 0x100;
	case IN_FTYPES: // Input file types
		return (U32)"VB audio files\0*.VB";
	//case IN_WTAG: // Input wave tag
	//	return 85;
	case OUT_FTYPES: // Output file types;
		return (U32)"Raw PCM files\0*.RAW";
	case FRAME_SIZE: // Maximum frame size
		return 0x4000;
	default:
		return 0;
	}
}

static int startingCount = 0;
ASIRESULT AILCALL FAR ASI_startup(void)
{
	// TODO: Implement
	return startingCount++ == 0 ? ASI_NOERR : ASI_ALREADY_STARTED;
}

ASIRESULT AILCALL FAR ASI_shutdown(void)
{
	// TODO: Implement
	if ( startingCount != 0 )
	{
		startingCount--;
		return ASI_NOERR;
	}
	else
	{
		return ASI_NOT_INIT;
	}
}

C8 FAR* AILCALL FAR ASI_error(void)
{
	// TODO: Implement
	return nullptr;
}

// ASI stream

bool tfile = false;
void FetchStr( ASISTREAM *STR, int offset = -1 )
{
	for (int i = 0; i < 2; i++)
	{
		STR->fetch_CB(STR->user, STR->channels[i].block_buffer, 0x2000, offset);
		if (offset != -1)
		{
			STR->channels[i].s_1 = 0.0;
			STR->channels[i].s_2 = 0.0;
		}
	}

	double f[5][2] = { { 0.0, 0.0 },
                {   60.0 / 64.0,  0.0 },
                {  115.0 / 64.0, -52.0 / 64.0 },
                {   98.0 / 64.0, -55.0 / 64.0 },
                {  122.0 / 64.0, -60.0 / 64.0 } };

	U8 *bufs[] = { STR->channels[0].block_buffer, STR->channels[1].block_buffer };
	S16 *dest[] = { STR->channels[0].frame, STR->channels[1].frame };
	for (int c = 0; c < 2; c++)
	{
		for ( int a = 0; a < 0x7000; a+=(28 * 2))
		{
			//if (buffer_size <= 0) break;
			int predict_nr = *(bufs[c]++);
			int shift_factor = predict_nr & 0xf;
			predict_nr >>= 4;
			int flags = *(bufs[c]++);
			if ( flags == 7 ) break;
			for ( int i = 0; i < 28; i += 2 )
			{
				int d = *(bufs[c]++);
				int s = ( d & 0xf ) << 12;
				if ( s & 0x8000 )
					s |= 0xffff0000;
				STR->channels[c].samples[i] = (double) ( s >> shift_factor  );
				s = ( d & 0xf0 ) << 8;
				if ( s & 0x8000 )
					s |= 0xffff0000;
				STR->channels[c].samples[i+1] = (double) ( s >> shift_factor  );

			}
			for ( int i = 0; i < 28; i++ )
			{
				STR->channels[c].samples[i] = STR->channels[c].samples[i] + STR->channels[c].s_1 * f[predict_nr][0] + STR->channels[c].s_2 * f[predict_nr][1];
				STR->channels[c].s_2 = STR->channels[c].s_1;
				STR->channels[c].s_1 = STR->channels[c].samples[i];
				int d = (int) ( STR->channels[c].samples[i] + 0.5 );
				*(dest[c]++) = (d & 0xffff);
			}
			if ( flags == 1 ) break; 
		}
	}
}

HASISTREAM AILCALL FAR ASI_stream_open(U32 user, AILASIFETCHCB fetch_CB, U32 total_size)
{
	ASISTREAM FAR *STR = (ASISTREAM FAR *) AIL_mem_alloc_lock((sizeof(ASISTREAM) + 15) & ~15);
	if (STR == NULL)
    {
		//strcpy(ASI_error_text,"Out of memory");
		return 0;
	}

	memset(STR, 0, sizeof(ASISTREAM));

	STR->user = user;
	STR->fetch_CB = fetch_CB;
	STR->size = total_size;

	STR->cursor = 0;
	STR->offset = 0;
	STR->cur_block = -1;
	STR->blocks = total_size / 0x4000;

	for (int i = 0; i < 2; i++)
	{
		STR->channels[i].s_1 = 0.0;
		STR->channels[i].s_2 = 0.0;
		memset(STR->channels[i].samples, 0, 28 * sizeof(double));
		memset(STR->channels[i].block_buffer, 0, 0x2000);
		memset(STR->channels[i].frame, 0, 0x3800);
	}

	return (HASISTREAM)STR;
}

ASIRESULT AILCALL ASI_stream_close(HASISTREAM stream)
{
	ASISTREAM FAR *STR = (ASISTREAM FAR *) stream;

	AIL_mem_free_lock(STR);

	return ASI_NOERR;
}

S32 AILCALL FAR ASI_stream_process(HASISTREAM stream, void FAR *buffer, S32 buffer_size)
{
	S32 tbuffer_size = buffer_size;

	ASISTREAM *STR = (ASISTREAM*)stream;

	if (GET_BLOCK(STR->offset) != STR->cur_block)
	{
		FetchStr(STR);
		STR->cur_block = GET_BLOCK(STR->offset);
	}


	S16 *dest = (S16 *)buffer;

	for (int i = 0; i < buffer_size / 2; i++)
	{
		for (int c = 0; c < 2; c++)
			dest[i * 2 + c] = STR->channels[c].frame[STR->cursor + i];
	}
	STR->cursor += buffer_size / 4;
	if (STR->cursor >= 0x3800)
	{
		STR->cursor = 0;
		STR->offset += 0x2000;
	}

	return buffer_size;
}

bool sfile = false;
ASIRESULT AILCALL ASI_stream_seek (HASISTREAM stream, S32 stream_offset)
{

	ASISTREAM *STR = (ASISTREAM*)stream;
	if (stream_offset > STR->size) return ASI_INVALID_PARAM;

	if (stream_offset != -2) // loop
		stream_offset = 0;

	stream_offset &= 0xFFFFE000;
	STR->offset = stream_offset;

	if (GET_BLOCK(STR->offset) != STR->cur_block)
	{
		FetchStr(STR, STR->offset);
		STR->cur_block = GET_BLOCK(STR->offset);
	}
	STR->cursor = 0;

   return ASI_NOERR;
}

S32 AILCALL FAR ASI_stream_attribute (HASISTREAM stream, HATTRIB attrib)
{
	ASISTREAM *STR = (ASISTREAM*)stream;
	switch (attrib)
	{
	case INPUT_BIT_RATE: return 0x7D00000;
	case INPUT_SAMPLE_RATE: return 1000;
	case INPUT_BITS: return 128;
	case INPUT_CHANNELS: return 2;
	case OUTPUT_BIT_RATE: return 32000 * 16 * 2;
	case OUTPUT_SAMPLE_RATE: return 32000;
	case OUTPUT_BITS: return 16;
	case OUTPUT_CHANNELS: return 2;
	case POSITION: return STR->offset;
	case PERCENT_DONE:
		{
			float percent = ((float)(100.0 * STR->offset) / (STR->size / 0x4000));
			return *(S32*)&percent;
		}
	case MIN_INPUT_BLOCK_SIZE: return 0x4000;
	}
	return -1;
}

S32 AILCALL FAR ASI_stream_set_preference (HASISTREAM stream, HATTRIB preference, void FAR* value)
{
	return 0;
}