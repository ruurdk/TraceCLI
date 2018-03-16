#include "precompiled.h"
#include "Debugger.h"
#include "IDebuggerImplementation.h"
#include "ManagedCallback.h"

using namespace std;
using namespace Microsoft::WRL;

#pragma once
class LegacyManagedDebugger : public IDebuggerImplementation
{
public:
	LegacyManagedDebugger(IDebugger* const debugger, DWORD pId);
	~LegacyManagedDebugger() override;
	ICorDebugProcess* const CorProcess(void) override;
	ICorDebugProcess5* const CorProcess5(void) override;
	void OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &Breakpoint) override;
	void OnException(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags) override;
private:
	IDebugger* debugger;

	ComPtr<ICLRDebugging> CLRDebug;				//.NET 4+ debug abstraction controller
	ComPtr<ICLRMetaHost> metahost;				//shim control
	ComPtr<ICLRRuntimeInfo> runtimeInfo;		//runtime info

	ComPtr<ICorDebugProcess> CorDebugProcess;
	ComPtr<ICorDebugProcess5> CorDebugProcess5;

	ComPtr<ICorDebug> CorDebug;

	HANDLE hProcess;
};

