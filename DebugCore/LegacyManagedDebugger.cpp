#include "precompiled.h"
#include "LegacyManagedDebugger.h"

LegacyManagedDebugger::LegacyManagedDebugger(IDebugger* const debugger, DWORD pId) : CorDebugProcess(nullptr), CorDebugProcess5(nullptr)
{
	this->debugger = debugger;

	TRACE(L"Start managed debugging\n");

	HRESULT hr;

	//get process handle
	hProcess = (HANDLE)OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE, false, pId);
	if (!hProcess) throw new exception("Cannot access process");

	//get CLR metahost
	hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, &metahost);
	if (hr != S_OK) throw new exception("Cannot access CLR shim library");

	//find runtime info about the target process
	ComPtr<IEnumUnknown> runtimeEnum;
	ComPtr<ICLRRuntimeInfo> tempInfo;
	hr = metahost->EnumerateLoadedRuntimes(hProcess, &runtimeEnum);
	if (hr != S_OK) throw new exception("Failed to enumerate process runtimes");
	for (; runtimeEnum->Next(1, &tempInfo, nullptr) == 0;)
	{
		if (tempInfo) runtimeInfo = tempInfo;
	}

	//get legacy cordebug interface
	hr = runtimeInfo->GetInterface(CLSID_CLRDebuggingLegacy, __uuidof(ICorDebug), &CorDebug);
	if (hr != S_OK) throw new exception("Failed to retreive legacy CLR debugging interface (ICorDebug)");
	hr = CorDebug->Initialize();
	if (hr != S_OK) throw new exception("Failed to initialize ICorDebug");

	//set managed handler
	auto handler = new ManagedCallback(this);	
	CorDebug->SetManagedHandler(handler);
	CorDebug->SetUnmanagedHandler(handler);

	//get the actual process interface
	hr = CorDebug->DebugActiveProcess(pId, false, &CorDebugProcess);
	if (hr != S_OK) throw new exception("Failed to attach to process");

	//disable optimizations (helps with walking the stack), but may be too late if already JITTED
	ComPtr<ICorDebugProcess2> pProcess2;
	if (CorDebugProcess.As(&pProcess2) == S_OK)
	{
		//ignore result, it's a bonus if it works
		pProcess2->SetDesiredNGENCompilerFlags(CORDEBUG_JIT_DISABLE_OPTIMIZATION);
	}

	//attach to already loaded app domains
	ComPtr<ICorDebugAppDomainEnum> appDomainEnum;
	hr = CorDebugProcess->EnumerateAppDomains(&appDomainEnum);
	if (hr != S_OK) throw new exception("Failed to enumerate process appdomains");

	ICorDebugAppDomain *loadedDomains[30]; //not sure how to do this with ComPtr
	ULONG numDomains;
	hr = appDomainEnum->Next(sizeof(loadedDomains), &loadedDomains[0], &numDomains);
	//if (hr != S_OK) throw new exception("Failed to get appdomains enumerator");

	wchar_t domainName[MAX_PATH];
	ULONG32 domainNameLen;
	BOOL isAttached;
	for (ULONG domainIt = 0; domainIt < numDomains; domainIt++)
	{
		loadedDomains[domainIt]->GetName(sizeof(domainName), &domainNameLen, domainName);		

		if ((loadedDomains[domainIt]->IsAttached(&isAttached) == S_OK) && !isAttached)
		{
			if (loadedDomains[domainIt]->Attach() != S_OK)
			{
				TRACE(L"Failed to attach to appDomain: %s\n", domainName);
			}
		}
		else 
		{
			if (isAttached) 
			{
				TRACE(L"Attached to existing appDomain: %s\n", domainName);
			}
			else 
			{
				TRACE(L"Failed to find Attached state of existing appDomain: %s\n", domainName);
			}
		}

		loadedDomains[domainIt]->Release();
	}

	//cache a v5 process interface
	CorDebugProcess.As(&CorDebugProcess5);

	TRACE(L"Managed debugger operational\n");
}

LegacyManagedDebugger::~LegacyManagedDebugger()
{
	TRACE(L"Tear down managed debugging\n");

	if (CorDebugProcess)
	{
		TRACE(L"Stop managed threads\n");
		CorDebugProcess->Stop(INFINITE);
		TRACE(L"Detaching\n");
		CorDebugProcess->Detach();
		TRACE(L"Detached\n");
		CorDebugProcess = nullptr;
	}

	CorDebugProcess5 = nullptr;

	VERIFY(CloseHandle(hProcess));

	CorDebug = nullptr;
	metahost = nullptr;
}

ICorDebugProcess* const LegacyManagedDebugger::CorProcess()
{
	ASSERT(CorDebugProcess);

	return CorDebugProcess.Get();
}

ICorDebugProcess5* const LegacyManagedDebugger::CorProcess5()
{
	ASSERT(CorDebugProcess5);

	return CorDebugProcess5.Get();
}

void LegacyManagedDebugger::OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &Breakpoint)
{	
	ASSERT(debugger);

	debugger->OnBreakpointHit(AppDomain, Thread, Breakpoint);
}

void LegacyManagedDebugger::OnException(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags)
{
	ASSERT(debugger);

	debugger->OnException(AppDomain, Thread, Frame, nOffset, dwEventType, dwFlags);
}