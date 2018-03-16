#include <vector>
#include <memory>

#include "..\Shared\DebugMode.h"

#pragma once

using std::vector;
using std::shared_ptr;

struct BPFilter
{
	wchar_t *namespaceFilter;
	wchar_t *classFilter;
	wchar_t *methodFilter;
	vector<const wchar_t*> fieldsToDump;

	~BPFilter()
	{
		if (namespaceFilter) delete[] namespaceFilter;
		if (classFilter) delete[] classFilter;
		if (methodFilter) delete[] methodFilter;
		for (auto fieldIt = fieldsToDump.begin(); fieldIt != fieldsToDump.end(); ++fieldIt)
		{
			delete[] *fieldIt;
		}
	}
};

struct Config
{
	vector<shared_ptr<BPFilter>> Breakpoints;
	OPMODE OperatingMode;
	wchar_t* OutfileName;

	~Config()
	{
		if (OutfileName) delete[] OutfileName;
	}
};

//contract for providing a config
struct IConfigProvider
{
	virtual Config* ProvideConfig() = 0;	
	virtual const wchar_t* HelpString() = 0;
};