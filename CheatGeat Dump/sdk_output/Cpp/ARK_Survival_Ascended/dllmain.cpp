#include "pch.h"

#include "SDK/Headers/BasicTypes_FUNCTIONS.h"
#include "SDK/Headers/BasicTypes_CLASSES.h"

// using namespace CG::BasicTypes;
// using namespace CG::CoreUObject;
// using namespace CG::Engine;

// - C++ Exceptions are /EHa (Yes with SEH Exceptions)

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved)
{
	DisableThreadLibraryCalls(hModule);

	if (ulReasonForCall != DLL_PROCESS_ATTACH)
		return TRUE;

	if (!CG::BasicTypes::InitSdk())
	{
		return FALSE;
	}

	// TODO: Fill your code here

	return TRUE;
}
