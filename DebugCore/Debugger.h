#include "precompiled.h"

#include "IDebuggerImplementation.h"
#include "IDebugger.h"
#include "SigParser.h"
#include "..\Shared\DebugMode.h"
#include "..\Shared\Logger.h"
#include "MemoryInfo.h"
#include "MetaHelpers.h"

#pragma once

struct PendingTimer
{
	long long timeStamp;
	DWORD threadId;
	mdMethodDef functionToken;
	shared_ptr<MethodInfo> method;
};

struct KnownException
{
	mdMethodDef thrownInMethod;
	DWORD thrownOnThreadId;
};

class Debugger : public IDebugger
{
public:
	Debugger(OPMODE mode);
	Debugger(OPMODE mode, DWORD pId);
	~Debugger();
	bool DoAttach(DWORD pId);
	void Detach();
	bool IsAttached() const;
	void FindManagedMethods(wchar_t * namespaceFilterName, const wchar_t * classFilterName, const wchar_t * methodFilterName, const vector<const wchar_t *> &fields, vector<shared_ptr<MethodInfo>> &functions);
	HRESULT SetBPAtEntry(shared_ptr<MethodInfo> pFunction, customHandler handler);
	int SetBPAtExit(shared_ptr<MethodInfo> pFunction, customHandler handler);
	int SetBPAtOpCode(shared_ptr<MethodInfo> pFunction, customHandler handler, CEE_OPCODE opcode);
	void ActivateBPs(BOOL active);
	void Continue();
	void Stop();
	void OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &Breakpoint) override;
	void OnException(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags) override;
	void GetBPStats(vector<shared_ptr<BreakpointInfo>> &BPStats);
	
	MemoryInfo* GetMemoryInfo();

	wchar_t* GetName(mdToken token) {
		return MetaInfo->GetName(token);
	}
private:
	DWORD pId;	
	ComPtr<IDebugClient> DebugClient;						//native debug client controller
	unique_ptr<IDebuggerImplementation> DebugClientManaged;	//managed debug client controller
	unique_ptr<MetaHelpers> MetaInfo;						//metadata resolver
	map<ICorDebugFunctionBreakpoint*, shared_ptr<BreakpointInfo>> managedBPs;

	OPMODE mode;

	HRESULT GetAppDomains(ICorDebugProcess *pProcess, ICorDebugAppDomain **ppDomains, ULONG maxDomains, ULONG *loadedDomains);
	HRESULT GetRuntimeAssemblies(ICorDebugAppDomain *pAppDomain, ICorDebugAssembly **ppAssemblies, ULONG maxAssemblies, ULONG *loadedAssemblies);
	HRESULT GetRuntimeModules(ICorDebugAssembly *pAssembly, ICorDebugModule **ppModules, ULONG maxModules, ULONG *loadedModules);

	HRESULT DumpFieldValue(const wchar_t* fieldSignature, ComPtr<ICorDebugValue> &pVal);
	HRESULT DereferenceIfPossible(ICorDebugValue **pVal);

	HRESULT TryGetStringFromObject(ComPtr<ICorDebugValue> &pObj, wchar_t *stringValue, ULONG32 maxChars, ULONG32 *actualChars);
	HRESULT GetConstValue(DWORD type, UVCP_CONSTANT fieldValue, ULONG fieldValueSize, wchar_t **constAsString);

	HRESULT GetCode(ICorDebugCode *code, ULONG32 bufferSize, byte* buffer, ULONG32 *numBytes);
	HRESULT ReadMemory(ICorDebugProcess *pProcess, CORDB_ADDRESS address, BYTE *pBuffer, ULONG32 bytesRequested, ULONG32 *pBytesRead);
	
	void LogExceptionDetails(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags);
	
	double timerFreq;
	map<ULONG64, PendingTimer> pendingTimers;
	vector<KnownException> knownExceptions;
};

