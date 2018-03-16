#include "Config.h"
#include <Windows.h>
#include <Shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#include <xmllite.h>
#pragma comment(lib,"xmllite.lib")
#include <wrl.h>

#pragma once
using Microsoft::WRL::ComPtr;

#include "..\Shared\stdstringsplit.h"

class FileConfigReader : public IConfigProvider
{
public:
	FileConfigReader(const wchar_t * fileName);
	~FileConfigReader();
	Config* ProvideConfig() override;	
	const wchar_t* HelpString() override;
private:
	bool ParseConfig(const wchar_t * fileName);
	shared_ptr<Config> config;
	const wchar_t* helpString = L"<Config timings=\"1\" stats=\"0\" outputfile=\"output.log\">\n\t<Filter namespace=\"System\" class=\"System.Object\" method=\"ToString\" fields=\"field1,field2\" />\n</Config>";
};

