#include "precompiled.h"
#include "ManagedCallback.h"

ManagedCallback::ManagedCallback(IDebuggerImplementation* debugger)
{
	ASSERT(debugger);

	pDebugger = debugger;
}

void ManagedCallback::Continue()
{
	ASSERT(pDebugger);

	pDebugger->CorProcess()->Continue(false);
}

HRESULT STDMETHODCALLTYPE ManagedCallback::QueryInterface(/* [in] */ REFIID riid, /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	ASSERT(ppvObject);

	if (riid == __uuidof(ICorDebugManagedCallback))
	{
		*ppvObject = static_cast<ICorDebugManagedCallback*>(this);
		return S_OK;
	}
	else if (riid == __uuidof(ICorDebugManagedCallback2))
	{
		*ppvObject = static_cast<ICorDebugManagedCallback2*>(this);
		return S_OK;
	}
	else if (riid == __uuidof(ICorDebugManagedCallback3))
	{
		*ppvObject = static_cast<ICorDebugManagedCallback3*>(this);
		return S_OK;
	}
	else if (riid == __uuidof(ICorDebugUnmanagedCallback)) 
	{
		*ppvObject = static_cast<ICorDebugUnmanagedCallback*>(this);
		return S_OK;
	}
	else if (riid == __uuidof(IUnknown))
	{
		*ppvObject = static_cast<ICorDebugManagedCallback*>(this);
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE ManagedCallback::AddRef(void)
{ 
	return 1; 
}

ULONG STDMETHODCALLTYPE ManagedCallback::Release(void)
{ 
	return 0; 
}

HRESULT STDMETHODCALLTYPE ManagedCallback::Breakpoint(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugBreakpoint *pBreakpoint)
{ 
	TRACE(L"Breakpoint hit\n");
	
	ASSERT(pAppDomain);
	ASSERT(pThread);
	ASSERT(pBreakpoint);

	try
	{
		ASSERT(pDebugger);

		pDebugger->OnBreakpointHit(*pAppDomain, *pThread, *pBreakpoint);
	}
	catch (...)
	{
		TRACE(L"Exception in breakpoint handler\n");
	}

	Continue();

	return S_OK; 
}

HRESULT STDMETHODCALLTYPE ManagedCallback::StepComplete(	/* [in] */ ICorDebugAppDomain *pAppDomain,/* [in] */ ICorDebugThread *pThread,	/* [in] */ ICorDebugStepper *pStepper,	/* [in] */ CorDebugStepReason reason)
{ 
	TRACE(L"StepCompleted\n");

	Continue();
	return S_OK; 
}

HRESULT STDMETHODCALLTYPE ManagedCallback::Break(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *thread)
{
	TRACE(L"Break instruction hit\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::Exception(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ BOOL unhandled)
{
	TRACE(L"%s exception\n", unhandled ? L"Unhandled" : L"First chance");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::EvalComplete(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugEval *pEval)
{ 
	TRACE(L"EvalCompleted\n");

	Continue();
	return S_OK; 
}

HRESULT STDMETHODCALLTYPE ManagedCallback::EvalException(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread,	/* [in] */ ICorDebugEval *pEval)
{ 
	TRACE(L"EvalException\n");

	Continue();
	return S_OK; 
}

HRESULT STDMETHODCALLTYPE ManagedCallback::CreateProcess(/* [in] */ ICorDebugProcess *pProcess)
{ 
	TRACE(L"ProcessCreated\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::ExitProcess(/* [in] */ ICorDebugProcess *pProcess)
{ 	 
	TRACE(L"ProcessExit\n");

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::CreateThread(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *thread)
{ 
	DWORD threadId = 0;
	thread->GetID(&threadId);

	TRACE(L"ThreadCreated: %u\n", threadId);

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::ExitThread(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *thread)
{ 
	DWORD threadId = 0;
	thread->GetID(&threadId);

	TRACE(L"ThreadExit: %u\n", threadId);

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::LoadModule(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugModule *pModule)
{ 
	wchar_t moduleName[2048];
	ULONG32 moduleNameLen;
	if (pModule->GetName(sizeof(moduleName), &moduleNameLen, moduleName) == S_OK)
	{
		TRACE(L"ModuleLoad: %s\n", moduleName);
	}
	else 
	{
		TRACE(L"ModuleLoad: ?\n");
	}
	//todo: enable class load notifications


	//disable optimizations
	ComPtr<ICorDebugModule2> Module2;
	if (pModule->QueryInterface(__uuidof(ICorDebugModule2), &Module2) == S_OK)
	{
		DWORD jitFlags;
		if (Module2->GetJITCompilerFlags(&jitFlags) == S_OK)
		{
			jitFlags |= CORDEBUG_JIT_DISABLE_OPTIMIZATION;
			Module2->SetJITCompilerFlags(jitFlags);
		}		
	}

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::UnloadModule(	/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugModule *pModule)
{ 
	wchar_t moduleName[2048];
	ULONG32 moduleNameLen;
	if (pModule->GetName(sizeof(moduleName), &moduleNameLen, moduleName) == S_OK)
	{
		TRACE(L"ModuleUNLoad: %s\n", moduleName);
	}
	else
	{
		TRACE(L"ModuleUNLoad: ?\n");
	}

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::LoadClass(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugClass *c)
{ 
	TRACE(L"ClassLoad\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::UnloadClass(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugClass *c)
{ 
	TRACE(L"ClassUnload\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::DebuggerError(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ HRESULT errorHR, /* [in] */ DWORD errorCode)
{ 
	TRACE(L"DebuggerError. Debugging services disabled.\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::LogMessage(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ LONG lLevel, /* [in] */ WCHAR *pLogSwitchName, /* [in] */ WCHAR *pMessage)
{ 
	TRACE(L"A thread used the System.Diagnostics.EventLog\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::LogSwitch(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread,	/* [in] */ LONG lLevel,	/* [in] */ ULONG ulReason, /* [in] */ WCHAR *pLogSwitchName, /* [in] */ WCHAR *pParentName)
{ 
	TRACE(L"A thread used the System.Diagnostics.Switch class\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::CreateAppDomain( /* [in] */ ICorDebugProcess *pProcess, /* [in] */ ICorDebugAppDomain *pAppDomain)
{ 
	wchar_t adName[2048];
	ULONG32 adNameLen;
	if (pAppDomain->GetName(sizeof(adName), &adNameLen, adName) == S_OK)
	{
		TRACE(L"AppDomainCreated: %s\n", adName);
	}
	else
	{
		TRACE(L"AppDomainCreated: <noname>\n");
	}

	pAppDomain->Attach();

	Continue(); 

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::ExitAppDomain(/* [in] */ ICorDebugProcess *pProcess,	/* [in] */ ICorDebugAppDomain *pAppDomain)
{ 
	wchar_t adName[2048];
	ULONG32 adNameLen;
	if (pAppDomain->GetName(sizeof(adName), &adNameLen, adName) == S_OK)
	{
		TRACE(L"AppDomainUNLoad: %s\n", adName);
	}
	else
	{
		TRACE(L"AppDomainUNLoad: <noname>\n");
	}

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::LoadAssembly(	/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugAssembly *pAssembly)
{ 
	wchar_t asmName[2048];
	ULONG32 asmNameLen;
	if (pAssembly->GetName(sizeof(asmName), &asmNameLen, asmName) == S_OK)
	{
		TRACE(L"AssemblyLoad: %s\n", asmName);
	}
	else
	{
		TRACE(L"AssemblyLoad: <noname>\n");
	}

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::UnloadAssembly(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugAssembly *pAssembly)
{ 
	wchar_t asmName[2048];
	ULONG32 asmNameLen;
	if (pAssembly->GetName(sizeof(asmName), &asmNameLen, asmName) == S_OK)
	{
		TRACE(L"AssemblyUNLoad: %s\n", asmName);
	}
	else
	{
		TRACE(L"AssemblyUNLoad: <noname>\n");
	}

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::ControlCTrap(	/* [in] */ ICorDebugProcess *pProcess)
{ 
	TRACE(L"A CTRL+C was trapped in the process\n");

	Continue(); 
	return S_OK; 
}

HRESULT STDMETHODCALLTYPE ManagedCallback::NameChange(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread)
{ 
	TRACE(L"An AppDomain and/or thread changed name\n");

	Continue();
	return S_OK; 
}

HRESULT STDMETHODCALLTYPE ManagedCallback::UpdateModuleSymbols( /* [in] */ ICorDebugAppDomain *pAppDomain,/* [in] */ ICorDebugModule *pModule, /* [in] */ IStream *pSymbolStream)
{ 
	TRACE(L"The symbols for a CLR module have changed\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::EditAndContinueRemap(	/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread,	/* [in] */ ICorDebugFunction *pFunction, /* [in] */ BOOL fAccurate)
{ 
	//deprecated event
	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::BreakpointSetError(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugBreakpoint *pBreakpoint,/* [in] */ DWORD dwError)
{ 
	TRACE(L"The common language runtime was unable to accurately bind a breakpoint that was set before a function was just - in - time(JIT) compiled\n");	

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::FunctionRemapOpportunity(/* [in] */ ICorDebugAppDomain *pAppDomain,/* [in] */ ICorDebugThread *pThread,/* [in] */ ICorDebugFunction *pOldFunction,/* [in] */ ICorDebugFunction *pNewFunction,	/* [in] */ ULONG32 oldILOffset) 
{ 
	TRACE(L"Code execution has reached a sequence point in an older version of an edited function\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::CreateConnection(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ CONNID dwConnectionId,	/* [in] */ WCHAR *pConnName) 
{ 
	TRACE(L"A new (debugger) connection has been created\n");	

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::ChangeConnection(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ CONNID dwConnectionId) 
{ 
	TRACE(L"The set of tasks associated with a debugger connection has changed\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::DestroyConnection(/* [in] */ ICorDebugProcess *pProcess, /* [in] */ CONNID dwConnectionId) 
{ 
	TRACE(L"A debugger connection has been terminated.\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::Exception(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugFrame *pFrame,	/* [in] */ ULONG32 nOffset,	/* [in] */ CorDebugExceptionCallbackType dwEventType, /* [in] */ DWORD dwFlags) 
{ 
	TRACE(L"A search for an exception handler has started.\n");

	try
	{
		ASSERT(pDebugger);

		pDebugger->OnException(*pAppDomain, *pThread, *pFrame, nOffset, dwEventType, dwFlags);
	}
	catch (...)
	{
		TRACE(L"Exception in exception handler\n");
	}

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::ExceptionUnwind(/* [in] */ ICorDebugAppDomain *pAppDomain, /* [in] */ ICorDebugThread *pThread, /* [in] */ CorDebugExceptionUnwindCallbackType dwEventType, /* [in] */ DWORD dwFlags) 
{ 
	TRACE(L"Exception unwind status info:\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::FunctionRemapComplete(/* [in] */ ICorDebugAppDomain *pAppDomain,	/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugFunction *pFunction) 
{ 
	TRACE(L"Code execution has switched to a new version of an edited function.\n");	

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::MDANotification(/* [in] */ ICorDebugController *pController,	/* [in] */ ICorDebugThread *pThread,/* [in] */ ICorDebugMDA *pMDA) 
{ 
	TRACE(L"Code execution has encountered a managed debugging assistant (MDA)\n");

	Continue(); 
	return S_OK;
}
//3
HRESULT STDMETHODCALLTYPE ManagedCallback::CustomNotification(/* [in] */ ICorDebugThread *pThread, /* [in] */ ICorDebugAppDomain *pAppDomain)
{ 
	TRACE(L"A custom debugger notification has been raised.\n");

	Continue(); 
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ManagedCallback::DebugEvent(/* [in] */ LPDEBUG_EVENT pDebugEvent,/* [in] */ BOOL fOutOfBand)
{
	TRACE(L"Unmanaged callback received\n");

	ASSERT(pDebugger);

	pDebugger->CorProcess()->Continue(fOutOfBand);

	return S_OK;
}