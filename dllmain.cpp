#include "vbdec.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	//UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(lpReserved);

	switch ( reason )
	{
	case DLL_PROCESS_ATTACH:
		RegisterVBInterface();
		break;
	case DLL_PROCESS_DETACH:
		UnregisterVBInterface();
		break;
	}
	return TRUE;
}