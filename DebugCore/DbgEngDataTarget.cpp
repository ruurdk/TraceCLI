#include "precompiled.h"
#include "DbgEngDataTarget.h"

DbgEngDataTarget::DbgEngDataTarget(IDebugClient * const debugClient, DWORD pId) : runtimeInfo(nullptr), CorDebugProcess(nullptr), CorDebugProcess5(nullptr)
{
	TRACE(L"Start managed debugging\n");

	ASSERT(debugClient);
	this->DebugClient = debugClient;

	HRESULT hr = S_OK;
	//get CLR debugging abstraction controller interface, & metahost
	hr |= CLRCreateInstance(CLSID_CLRDebugging, IID_ICLRDebugging, &CLRDebug);
	hr |= CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, &Metahost);
	if (hr != S_OK) throw new exception("Failed to retreive CLR interface");

	//cache interfaces that will be needed later
	hr |= DebugClient.As(&DataSpaces);
	hr |= DebugClient.As(&Symbols);
	hr |= DebugClient.As(&Advanced);
	hr |= DebugClient.As(&SysObjs);	
	if (hr != S_OK) throw new exception("Failed to retreive native debugging interface");

	//get process handle
	hProcess = (HANDLE)OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE, false, pId);
	if (!hProcess) throw new exception("Failed to open process");

	//find runtime info about the target process
	ComPtr<IEnumUnknown> runtimeEnum;
	ComPtr<ICLRRuntimeInfo> tempInfo;
	hr = Metahost->EnumerateLoadedRuntimes(hProcess, &runtimeEnum);
	if (hr != S_OK) throw new exception("Failed to enumerate process runtimes");
	for (; runtimeEnum->Next(1, &tempInfo, nullptr) == 0;)
	{
		if (tempInfo) runtimeInfo = tempInfo;
	}
	if (!runtimeInfo) throw new exception("Failed to retreive process runtime info");

	//max version
	CLR_DEBUGGING_VERSION maxVersion = CLR_DEBUGGING_VERSION{};
	maxVersion.wMajor = 5;
	maxVersion.wMinor = 0;
	maxVersion.wBuild = 0;
	maxVersion.wRevision = 0;

	//retreive a ICorDebugProcess interface for this process
	if (hr != S_OK) throw new exception("Failed to connect managed debugging abstraction");

	//try to cache a v5 interface of this type
	hr = CorDebugProcess.As(&CorDebugProcess5);
	if (hr != S_OK)
	{
		TRACE(L"Failed to retreive ICorDebugProcess5 interface, managed debugging is limited\n");
	}

	TRACE(L"Managed debugger operational\n");
}

DbgEngDataTarget::~DbgEngDataTarget()
{
	TRACE(L"Tear down managed debugging\n");

	CorDebugProcess = nullptr;

	VERIFY(CloseHandle(hProcess));

	DataSpaces = nullptr;
	Symbols = nullptr;
	Advanced = nullptr;
	SysObjs = nullptr;

	runtimeInfo = nullptr;
	Metahost = nullptr;

	for (auto it = libraries.begin(); it != libraries.end(); ++it)
	{
		if (CLRDebug->CanUnloadNow(*it) == S_OK)
		{
			VERIFY(FreeLibrary(*it));
		}
	}
	VERIFY(libraries.empty());

	CLRDebug = nullptr;
}

ICorDebugProcess* const DbgEngDataTarget::CorProcess()
{
	return CorDebugProcess.Get();
}

ICorDebugProcess5* const DbgEngDataTarget::CorProcess5()
{
	return CorDebugProcess5.Get();
}

ULONG64 DbgEngDataTarget::FindCLRBaseAddress()
{
	HMODULE modList[1024];
	DWORD loadedMods;
	auto hr = EnumProcessModules(hProcess, modList, sizeof(modList), &loadedMods);
	if (!hr)
	{
		TRACE(L"Failed to enumerate process modules\n");
		return 0;
	}

	MODULEINFO baseMod;
	wchar_t baseName[MAX_PATH];
	for (DWORD nMod = 0; nMod < loadedMods; nMod++)
	{
		if (GetModuleBaseName(hProcess, modList[nMod], baseName, MAX_PATH) != 0)
		{
			if (wcscmp(baseName, L"clr.dll") == 0)
			{
				hr = GetModuleInformation(hProcess, modList[nMod], &baseMod, sizeof(MODULEINFO));
				if (hr)	return (ULONG64)baseMod.lpBaseOfDll;

				TRACE(L"Failed to get CLR module information\n");
				return 0;
			}
		}
	}
	return 0;
}

HRESULT STDMETHODCALLTYPE DbgEngDataTarget::QueryInterface(/* [in] */ REFIID riid, /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	ASSERT(ppvObject);

	if ((riid == IID_ICorDebugDataTarget) || (riid == IID_ICLRDebuggingLibraryProvider) || (riid == IID_IUnknown))
	{
		*ppvObject = this;
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DbgEngDataTarget::AddRef(void)
{
	return 1;
}

ULONG STDMETHODCALLTYPE DbgEngDataTarget::Release(void)
{
	return 0;
}

HRESULT STDMETHODCALLTYPE DbgEngDataTarget::GetPlatform(/* [out] */ CorDebugPlatform *pTargetPlatform)
{
	ASSERT(pTargetPlatform);

	SYSTEM_INFO info;
	GetNativeSystemInfo(&info);
	if (info.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_AMD64 && info.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_INTEL) return E_FAIL;

	auto OS64 = info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
	if (!OS64)
	{
		*pTargetPlatform = CorDebugPlatform::CORDB_PLATFORM_WINDOWS_X86;
	}
	else
	{
		BOOL is32on64 = false;
		VERIFY(IsWow64Process(hProcess, &is32on64));
		*pTargetPlatform = is32on64 ? CorDebugPlatform::CORDB_PLATFORM_WINDOWS_X86 : CorDebugPlatform::CORDB_PLATFORM_WINDOWS_AMD64;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DbgEngDataTarget::ReadVirtual(/* [in] */ CORDB_ADDRESS address,	/* [length_is][size_is][out] */ BYTE *pBuffer,	/* [in] */ ULONG32 bytesRequested,	/* [out] */ ULONG32 *pBytesRead)
{
	ASSERT(pBuffer);
	ASSERT(pBytesRead);

	return ReadProcessMemory(hProcess, (void*)address, pBuffer, bytesRequested, (SIZE_T*)pBytesRead) != 0 ? S_OK : E_FAIL;
	//auto hr = pDataSpaces->ReadPhysical(address, pBuffer, bytesRequested, &bytesRead);	
}

HRESULT STDMETHODCALLTYPE DbgEngDataTarget::GetThreadContext(/* [in] */ DWORD dwThreadID,	/* [in] */ ULONG32 contextFlags, /* [in] */ ULONG32 contextSize, /* [size_is][out] */ BYTE *pContext)
{
	ASSERT(pContext);

	//can't seem to get this through debugging interfaces, so resort to win32
	auto hThread = OpenThread(THREAD_GET_CONTEXT, false, dwThreadID);
	if (!hThread)
	{
		TRACE(L"Failed to open thread with Id %u\n", dwThreadID);
		return E_FAIL;
	}

	auto lpContext = (CONTEXT*)pContext;
	lpContext->ContextFlags = contextFlags;

	if (!::GetThreadContext(hThread, lpContext))
	{
		TRACE(L"Failed to get thread context\n");
		return E_FAIL;
	}

	VERIFY(CloseHandle(hThread));

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DbgEngDataTarget::ProvideLibrary(/* [in] */ const WCHAR *pwszFileName,	/* [in] */ DWORD dwTimestamp, /* [in] */ DWORD dwSizeOfImage, /* [out] */ HMODULE *phModule)
{
	ASSERT(pwszFileName);
	ASSERT(phModule);

	wchar_t runtimeFolder[MAX_PATH];
	DWORD pathLen = sizeof(runtimeFolder);
	auto hr = runtimeInfo->GetRuntimeDirectory(runtimeFolder, &pathLen);
	if (hr != S_OK)
	{
		TRACE(L"Failed to get runtimeFoler from ICLRRuntimeInfo\n");
		return E_FAIL;
	}

	VERIFY(wcscat_s(runtimeFolder, pwszFileName) == 0);

	auto hLib = LoadLibrary(runtimeFolder);
	if (!hLib)
	{
		TRACE(L"Failed to load library: %s\n", runtimeFolder);
		return E_FAIL;
	}

	libraries.push_back(hLib);
	*phModule = hLib;

	TRACE(L"Provided library: %s\n", runtimeFolder);

	return S_OK;

}

//this will never be called
void DbgEngDataTarget::OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &Breakpoint)
{
	return;
}