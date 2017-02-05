#include "vagdec.h"

#include <cstdlib>

static HPROVIDER providerHandle;
void RegisterVAGInterface()
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

void UnRegisterVAGInterface()
{
	RIB_unregister_all(providerHandle);
}

// ASI codec

U32 AILCALL FAR PROVIDER_query_attribute(HATTRIB index)
{
	switch ( index )
	{
	case PROVIDER_NAME:
		return (U32)"MSS VAG Audio Decoder";
	case PROVIDER_VERSION:
		return 0x100;
	case IN_FTYPES: // Input file types
		return (U32)"VAG audio files\0*.VAG";
	//case IN_WTAG: // Input wave tag
	//	return 85;
	case OUT_FTYPES: // Output file types;
		return (U32)"Raw PCM files\0*.RAW";
	case FRAME_SIZE: // Maximum frame size
		return 2 * 16 * 512;
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

bool FetchStr( ASISTREAM *STR, int offset = -1 )
{
	U8 in_buf[0x20];

	if ( STR->loop )
	{
		offset = sizeof(STR->VAGheader);
		STR->loop = false;
	}
	
	auto bytes_read = STR->fetch_CB(STR->user, in_buf, STR->num_of_channels * 0x10, offset);
	if (bytes_read < STR->num_of_channels * 0x10) return false;

	if (*(U32*)&in_buf[0] == 'pGAV') // hooray loop
	{
		bytes_read = STR->fetch_CB(STR->user, in_buf, STR->num_of_channels * 0x10, sizeof(STR->VAGheader));
		if (bytes_read < STR->num_of_channels * 0x10) return false;
	}
		
	for (int i = 0; i < STR->num_of_channels; i++)
	{
		if (offset != -1)
		{
			STR->channels[i].s_1 = 0.0;
			STR->channels[i].s_2 = 0.0;
		}
	}

	static const double f[5][2] = { { 0.0, 0.0 },
                {   60.0 / 64.0,  0.0 },
                {  115.0 / 64.0, -52.0 / 64.0 },
                {   98.0 / 64.0, -55.0 / 64.0 },
                {  122.0 / 64.0, -60.0 / 64.0 } };

	U8 *bufs[] = { in_buf, &in_buf[0x10] };
	S16 *dest[] = { STR->channels[0].decoded_samples, STR->channels[1].decoded_samples };
	for (int c = 0; c < STR->num_of_channels; c++)
	{
		int predict_nr = *(bufs[c]++);
		int shift_factor = predict_nr & 0xf;
		predict_nr >>= 4;
		int flags = *(bufs[c]++);
		if ( flags == 7 ) return false;
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
			if ( d & 0x8000 ) d |= 0xffff0000;
			*(dest[c]++) = (d & 0xffff);
		}
		if ( flags == 1 ) break; 
	}
	return true;
}


HASISTREAM AILCALL FAR ASI_stream_open(U32 user, AILASIFETCHCB fetch_CB, U32 total_size)
{
	ASISTREAM FAR *STR = (ASISTREAM FAR *) AIL_mem_alloc_lock( sizeof(ASISTREAM) );
	if (STR == nullptr)
    {
		return 0;
	}

	memset(STR, 0, sizeof(*STR));

	STR->user = user;
	STR->fetch_CB = fetch_CB;
	STR->size = total_size;

	fetch_CB(user, &STR->VAGheader, sizeof(STR->VAGheader), -1);
	STR->VAGheader.version = _byteswap_ulong(STR->VAGheader.version);
	STR->VAGheader.sample_rate = _byteswap_ulong(STR->VAGheader.sample_rate);
	STR->VAGheader.size = _byteswap_ulong(STR->VAGheader.size);
	STR->num_of_channels = (STR->VAGheader.stereo != 0) ? 2 : 1;

	FetchStr(STR, sizeof(STR->VAGheader));
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
	ASISTREAM *STR = (ASISTREAM*)stream;
	
	S16 *dest = (S16 *)buffer;
	S32 bytes_decoded = 0;
	
	for (int i = 0; i < buffer_size / (2 * STR->num_of_channels); i++)
	{
		for (int c = 0; c < STR->num_of_channels; c++)
		{
			dest[i * STR->num_of_channels + c] = STR->channels[c].decoded_samples[STR->cursor];
			bytes_decoded += sizeof(S16);
		}
		STR->cursor++;
		if (STR->cursor >= 28)
		{
			STR->cursor = 0;
			if (!FetchStr(STR))
				break;
		}
	}
	STR->offset += bytes_decoded;
	return bytes_decoded;
}

ASIRESULT AILCALL ASI_stream_seek (HASISTREAM stream, S32 stream_offset)
{
	ASISTREAM *STR = (ASISTREAM*)stream;
	if (stream_offset > (S32)(STR->VAGheader.size + sizeof(STR->VAGheader))) return ASI_INVALID_PARAM;

	if (stream_offset == -2) // loop
	{
		stream_offset = sizeof(STR->VAGheader);
		STR->loop = true;
	}
	else
	{
		STR->offset = stream_offset;
		STR->cursor = 0;
		FetchStr(STR, stream_offset);
	}

	return ASI_NOERR;
}

S32 AILCALL FAR ASI_stream_attribute (HASISTREAM stream, HATTRIB attrib)
{
	ASISTREAM *STR = (ASISTREAM*)stream;
	switch (attrib)
	{
	case INPUT_BIT_RATE: return 0x10 * 1000 * STR->num_of_channels * 8; // align 0x10 bytes * channels
	case INPUT_SAMPLE_RATE: return STR->VAGheader.sample_rate;
	case INPUT_BITS: return 4;
	case INPUT_CHANNELS: return STR->num_of_channels;
	case OUTPUT_BIT_RATE: return STR->VAGheader.sample_rate * 16 * STR->num_of_channels;
	case OUTPUT_SAMPLE_RATE: return STR->VAGheader.sample_rate;
	case OUTPUT_BITS: return 16;
	case OUTPUT_CHANNELS: return STR->num_of_channels;
	case POSITION: return STR->offset;
	case PERCENT_DONE:
		{
			float percent = ((float)(100.0 * STR->offset) / (STR->size / (STR->num_of_channels * 16 * 512)));
			return *(S32*)&percent;
		}
	case MIN_INPUT_BLOCK_SIZE: return STR->num_of_channels * 16 * 512;
	}
	return -1;
}

S32 AILCALL FAR ASI_stream_set_preference (HASISTREAM stream, HATTRIB preference, void FAR* value)
{
	return 0;
}