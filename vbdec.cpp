#include "vbdec.h"


static HPROVIDER providerHandle;
void RegisterVBInterface()
{
	// ASI codec
	const RIB_INTERFACE_ENTRY codecEntries[] = {
		REG_FN(PROVIDER_query_attribute),
		REG_AT("Name", PROVIDER_NAME, RIB_STRING),
		REG_AT("Version", PROVIDER_VERSION, RIB_HEX),
		REG_AT("Input file types", 0, RIB_STRING),
		REG_AT("Input wave tag", 1, RIB_DEC),
		REG_AT("Output file types", 2, RIB_STRING),
		REG_AT("Maximum frame size", 3, RIB_DEC),
		REG_FN(ASI_startup),
		REG_FN(ASI_error),
		REG_FN(ASI_shutdown),
	};

	// ASI stream
	const RIB_INTERFACE_ENTRY streamEntries[] = { 
		REG_FN(ASI_stream_open),
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
	case 0: // Input file types
		return (U32)"VB audio files\0*.VB";
	case 1: // Input wave tag
		return 85;
	case 2: // Output file types;
		return (U32)"Raw PCM files\0*.RAW";
	case 3: // Maximum frame size
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
	return 0;
}