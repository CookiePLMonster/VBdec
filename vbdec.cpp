#include "vbdec.h"


static HPROVIDER providerHandle;
void RegisterVBInterface()
{
	// ASI codec
	const RIB_INTERFACE_ENTRY codecEntries[] = {
		REG_FN(PROVIDER_query_attribute),
		REG_AT("Name", PROVIDER_NAME, RIB_STRING),
		REG_AT("Version", PROVIDER_VERSION, RIB_HEX),
		REG_AT("Input file types", PROVIDER_InputFileTypes, RIB_STRING),
		REG_AT("Input wave tag", PROVIDER_InputWaveTag, RIB_DEC),
		REG_AT("Output file types", PROVIDER_OutputFileTypes, RIB_STRING),
		REG_AT("Maximum frame size", PROVIDER_MaxFrameSize, RIB_DEC),
		REG_FN(ASI_startup),
		REG_FN(ASI_error),
		REG_FN(ASI_shutdown),
	};

	// ASI stream
	const RIB_INTERFACE_ENTRY streamEntries[] = { 
		REG_FN(ASI_stream_open),
		REG_FN(ASI_stream_process),
		REG_FN(ASI_stream_attribute),
		REG_FN(ASI_stream_set_preference),
		REG_FN(ASI_stream_seek), 
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
	case PROVIDER_InputFileTypes:
		return (U32)"VB audio files\0*.VB";
	case PROVIDER_InputWaveTag:
		return 85;
	case PROVIDER_OutputFileTypes:
		return (U32)"Raw PCM files\0*.RAW";
	case PROVIDER_MaxFrameSize:
		return 0x1000;
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

HASISTREAM AILCALL FAR ASI_stream_open(U32 user, AILASIFETCHCB fetch_CB, U32 total_size)
{
	void* stream = AIL_mem_alloc_lock( 4 );
	return (HASISTREAM)stream;
}

S32 AILCALL FAR ASI_stream_process(HASISTREAM  stream, void FAR *buffer, S32 buffer_size)
{

	return 0;
}

S32 AILCALL FAR ASI_stream_attribute(HASISTREAM stream, HATTRIB attrib)
{

	return -1;
}

S32 AILCALL FAR ASI_stream_set_preference(HASISTREAM stream, HATTRIB preference, void FAR* value)
{

	return -1;
}

ASIRESULT AILCALL FAR ASI_stream_seek(HASISTREAM stream, S32 stream_offset)
{

	return ASI_NOERR;
}

ASIRESULT AILCALL FAR ASI_stream_close(HASISTREAM stream)
{
	void* mem = (void*)stream;
	AIL_mem_free_lock( mem );

	return ASI_NOERR;
}