#include "precompiled.h"
#include "IDebuggerImplementation.h"

using namespace std;
using namespace Microsoft::WRL;

#pragma once
class DbgEngDataTarget : public ICorDebugDataTarget, ICLRDebuggingLibraryProvider, IDebuggerImplementation
{
public:
	//IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(/* [in] */ REFIID riid, /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override;
	ULONG STDMETHODCALLTYPE AddRef(void) override;
	ULONG STDMETHODCALLTYPE Release(void) override;
	//ICorDebugDataTarget
	HRESULT STDMETHODCALLTYPE GetPlatform(/* [out] */ CorDebugPlatform *pTargetPlatform) override;
	HRESULT STDMETHODCALLTYPE ReadVirtual(/* [in] */ CORDB_ADDRESS address,	/* [length_is][size_is][out] */ BYTE *pBuffer,	/* [in] */ ULONG32 bytesRequested,	/* [out] */ ULONG32 *pBytesRead) override;
	HRESULT STDMETHODCALLTYPE GetThreadContext(/* [in] */ DWORD dwThreadID,	/* [in] */ ULONG32 contextFlags, /* [in] */ ULONG32 contextSize, /* [size_is][out] */ BYTE *pContext) override;
	//ICLRDebuggingLibraryProvider
	HRESULT STDMETHODCALLTYPE ProvideLibrary(/* [in] */ const WCHAR *pwszFileName,	/* [in] */ DWORD dwTimestamp, /* [in] */ DWORD dwSizeOfImage, /* [out] */ HMODULE *phModule) override;

	DbgEngDataTarget(IDebugClient * const debugClient, DWORD pId);
	~DbgEngDataTarget() override;
	ICorDebugProcess* const CorProcess(void) override;
	ICorDebugProcess5* const CorProcess5(void) override;
	void OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &pBreakpoint) override;
private:
	ComPtr<IDebugClient> DebugClient;				//dbgeng native debug controller

	ComPtr<ICLRDebugging> CLRDebug;				//.NET 4+ debug abstraction controller
	ComPtr<ICLRMetaHost> Metahost;				//shim control
	ComPtr<ICLRRuntimeInfo> runtimeInfo;			//runtime info

	ComPtr<ICorDebugProcess> CorDebugProcess;
	ComPtr<ICorDebugProcess5> CorDebugProcess5;

	ComPtr<IDebugDataSpaces2> DataSpaces;
	ComPtr<IDebugSymbols> Symbols;
	ComPtr<IDebugAdvanced> Advanced;
	ComPtr<IDebugSystemObjects> SysObjs;

	HANDLE hProcess;
	ULONG64 FindCLRBaseAddress();

	CLR_DEBUGGING_VERSION CLRVersion = CLR_DEBUGGING_VERSION {};
	CLR_DEBUGGING_PROCESS_FLAGS VersionFlags = CLR_DEBUGGING_PROCESS_FLAGS {};
	vector<HMODULE> libraries;
};

