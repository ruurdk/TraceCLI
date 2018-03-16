#pragma once
class ProcessInfo
{
public:
	HRESULT GetProcesses(bool onlyManaged, vector<shared_ptr<ProcInfo>> &info);
	HRESULT GetInstalledRuntimes(vector<shared_ptr<RuntimeInfo>> &info);
};