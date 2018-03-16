#include "precompiled.h"
#include "IDebuggerImplementation.h"

using namespace Microsoft::WRL;

#pragma once

class ManagedCallback : public ICorDebugManagedCallback, public ICorDebugManagedCallback2, public ICorDebugManagedCallback3, public ICorDebugUnmanagedCallback
{
public:
	ManagedCallback(IDebuggerImplementation* debugger);
	//IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(/* [in] */ REFIID riid, /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);
	ULONG STDMETHODCALLTYPE AddRef(void);
	ULONG STDMETHODCALLTYPE Release(void);
	//1
	HRESULT STDMETHODCALLTYPE Breakpoint(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugBreakpoint *pBreakpoint);
	HRESULT STDMETHODCALLTYPE StepComplete(	/* [in] */ ICorDebugAppDomain *pAppDomain,/* [in] */ ICorDebugThread *pThread,	/* [in] */ ICorDebugStepper *pStepper,	/* [in] */ CorDebugStepReason reason);
	HRESULT STDMETHODCALLTYPE Break(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *thread);
	HRESULT STDMETHODCALLTYPE Exception(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ BOOL unhandled);
	HRESULT STDMETHODCALLTYPE EvalComplete(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugEval *pEval);
	HRESULT STDMETHODCALLTYPE EvalException(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread,	/* [in] */ ICorDebugEval *pEval);
	HRESULT STDMETHODCALLTYPE CreateProcess(/* [in] */ ICorDebugProcess *pProcess);
	HRESULT STDMETHODCALLTYPE ExitProcess(/* [in] */ ICorDebugProcess *pProcess);
	HRESULT STDMETHODCALLTYPE CreateThread(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *thread);
	HRESULT STDMETHODCALLTYPE ExitThread(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *thread);
	HRESULT STDMETHODCALLTYPE LoadModule(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugModule *pModule);
	HRESULT STDMETHODCALLTYPE UnloadModule(	/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugModule *pModule);
	HRESULT STDMETHODCALLTYPE LoadClass(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugClass *c);
	HRESULT STDMETHODCALLTYPE UnloadClass(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugClass *c);
	HRESULT STDMETHODCALLTYPE DebuggerError(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ HRESULT errorHR, /* [in] */ DWORD errorCode);
	HRESULT STDMETHODCALLTYPE LogMessage(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ LONG lLevel, /* [in] */ WCHAR *pLogSwitchName, /* [in] */ WCHAR *pMessage);
	HRESULT STDMETHODCALLTYPE LogSwitch(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread,	/* [in] */ LONG lLevel,	/* [in] */ ULONG ulReason, /* [in] */ WCHAR *pLogSwitchName, /* [in] */ WCHAR *pParentName);
	HRESULT STDMETHODCALLTYPE CreateAppDomain( /* [in] */ ICorDebugProcess *pProcess, /* [in] */ ICorDebugAppDomain *pAppDomain);
	HRESULT STDMETHODCALLTYPE ExitAppDomain(/* [in] */ ICorDebugProcess *pProcess,	/* [in] */ ICorDebugAppDomain *pAppDomain);
	HRESULT STDMETHODCALLTYPE LoadAssembly(	/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugAssembly *pAssembly);
	HRESULT STDMETHODCALLTYPE UnloadAssembly(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugAssembly *pAssembly);
	HRESULT STDMETHODCALLTYPE ControlCTrap(	/* [in] */ ICorDebugProcess *pProcess);
	HRESULT STDMETHODCALLTYPE NameChange(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread);
	HRESULT STDMETHODCALLTYPE UpdateModuleSymbols( /* [in] */ ICorDebugAppDomain *pAppDomain,/* [in] */ ICorDebugModule *pModule, /* [in] */ IStream *pSymbolStream);
	HRESULT STDMETHODCALLTYPE EditAndContinueRemap(	/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread,	/* [in] */ ICorDebugFunction *pFunction, /* [in] */ BOOL fAccurate);
	HRESULT STDMETHODCALLTYPE BreakpointSetError(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugBreakpoint *pBreakpoint,/* [in] */ DWORD dwError);
	//2
	HRESULT STDMETHODCALLTYPE FunctionRemapOpportunity(/* [in] */ ICorDebugAppDomain *pAppDomain,/* [in] */ ICorDebugThread *pThread,/* [in] */ ICorDebugFunction *pOldFunction,/* [in] */ ICorDebugFunction *pNewFunction,	/* [in] */ ULONG32 oldILOffset);
	HRESULT STDMETHODCALLTYPE CreateConnection(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ CONNID dwConnectionId,	/* [in] */ WCHAR *pConnName);
	HRESULT STDMETHODCALLTYPE ChangeConnection(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ CONNID dwConnectionId);
	HRESULT STDMETHODCALLTYPE DestroyConnection(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ CONNID dwConnectionId);
	HRESULT STDMETHODCALLTYPE Exception(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugFrame *pFrame,	/* [in] */ ULONG32 nOffset,	/* [in] */ CorDebugExceptionCallbackType dwEventType, /* [in] */ DWORD dwFlags);
	HRESULT STDMETHODCALLTYPE ExceptionUnwind(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread, /* [in] */ CorDebugExceptionUnwindCallbackType dwEventType, /* [in] */ DWORD dwFlags);
	HRESULT STDMETHODCALLTYPE FunctionRemapComplete(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugFunction *pFunction);
	HRESULT STDMETHODCALLTYPE MDANotification(/* [in] */ ICorDebugController *pController,	/* [in] */ ICorDebugThread *pThread,/* [in] */ ICorDebugMDA *pMDA);
	//3
	HRESULT STDMETHODCALLTYPE CustomNotification(/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugAppDomain *pAppDomain);
	//unmanaged
	HRESULT STDMETHODCALLTYPE DebugEvent(/* [in] */ LPDEBUG_EVENT pDebugEvent,/* [in] */ BOOL fOutOfBand);
private:
	IDebuggerImplementation* pDebugger;
	void Continue();
};