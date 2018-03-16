#include "precompiled.h"
#include "ProcessInfo.h"

HRESULT ProcessInfo::GetProcesses(bool onlyManaged, vector<shared_ptr<ProcInfo>> &info)
{
	//get CLR metahost
	ComPtr<ICLRMetaHost> metahost;
	auto hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, &metahost);
	if (hr != S_OK) return hr;

	//list processes
#define MAXPROCESSES 1024
	ULONG maxProcesses = MAXPROCESSES;
	ULONG loadedProcesses;
	DWORD procIds[MAXPROCESSES];
#undef MAXPROCESSES
	if (!EnumProcesses(procIds, maxProcesses, &loadedProcesses)) return E_FAIL;

#define MAXPROCESSNAMELEN 2048
	ULONG32 maxProcessNameLen = MAXPROCESSNAMELEN;
	wchar_t procName[MAXPROCESSNAMELEN];
#undef MAXPROCESSNAMELEN
	DWORD processNameLen;

	//get info
	for (ULONG i = 0; i < loadedProcesses; i++)
	{
		//get process handle
		auto hProcess = (HANDLE)OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, procIds[i]);
		if (!hProcess) continue;

		auto procInfo = shared_ptr<ProcInfo>(new ProcInfo{});
		procInfo->pId = procIds[i];

		//find runtime info about the target process
		ComPtr<IEnumUnknown> runtimeEnum;
		ComPtr<ICLRRuntimeInfo> tempInfo;
		hr = metahost->EnumerateLoadedRuntimes(hProcess, &runtimeEnum);
		if (hr == S_OK) 
		{
			for (; runtimeEnum->Next(1, &tempInfo, nullptr) == 0;)
			{
				if (!tempInfo) continue;

				procInfo->numRuntimes++;
			}
		}

		if (onlyManaged && (procInfo->numRuntimes == 0)) continue;

		processNameLen = maxProcessNameLen;
		if (QueryFullProcessImageName(hProcess, 0, procName, &processNameLen))
		{
			procInfo->displayName = new wchar_t[processNameLen + 2];
			VERIFY(wcscpy_s(procInfo->displayName, processNameLen + 1, procName) == 0);
		}
		else 
		{
			procInfo->displayName = L"";
		}
		
		VERIFY(CloseHandle(hProcess));

		//add to result
		info.push_back(procInfo);
	}

	return S_OK;
}

HRESULT ProcessInfo::GetInstalledRuntimes(vector<shared_ptr<RuntimeInfo>> &info)
{
	//get CLR metahost
	ComPtr<ICLRMetaHost> metahost;
	auto hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, &metahost);
	if (hr != S_OK) return hr;

	ComPtr<IEnumUnknown> runtimeEnum;
	hr = metahost->EnumerateInstalledRuntimes(&runtimeEnum);
	if (hr != S_OK) return hr;

	ULONG loadedThisNext;
	ComPtr<ICLRRuntimeInfo> runtimeInfo;
	while ((runtimeEnum->Next(1, &runtimeInfo, &loadedThisNext) == S_OK) && loadedThisNext)
	{
		auto runtimeRecord = shared_ptr<RuntimeInfo>(new RuntimeInfo{});


#define VERSIONSTRINGLEN 100
		DWORD maxVersionLen = VERSIONSTRINGLEN;
		wchar_t versionString[VERSIONSTRINGLEN];
#undef VERSIONSTRINGLEN
		if (runtimeInfo->GetVersionString(versionString, &maxVersionLen) == S_OK)
		{
			runtimeRecord->versionString = new wchar_t[maxVersionLen + 2];
			VERIFY(wcscpy_s(runtimeRecord->versionString, maxVersionLen + 1, versionString) == 0);
		}

		wchar_t runtimeFolder[MAX_PATH + 2];
		DWORD maxRuntimeFolderLen = MAX_PATH + 1;
		if (runtimeInfo->GetRuntimeDirectory(runtimeFolder, &maxRuntimeFolderLen) == S_OK)
		{
			runtimeRecord->runtimeFolder = new wchar_t[maxRuntimeFolderLen + 2];
			VERIFY(wcscpy_s(runtimeRecord->runtimeFolder, maxRuntimeFolderLen + 1, runtimeFolder) == 0);
		}

		info.push_back(runtimeRecord);
	}

	return S_OK;
}