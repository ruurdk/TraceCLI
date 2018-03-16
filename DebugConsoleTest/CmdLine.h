#include <boost\program_options.hpp>
#include <iostream>
#include <Windows.h>
#include "..\Shared\DebugMode.h"
#include "version.h"
#include "Config.h"
#include "..\Shared\Logger.h"

#pragma once

namespace po = boost::program_options;

class CmdLine : public IConfigProvider
{
public:
	CmdLine(int argc, wchar_t * argv[]);
	
	po::variables_map vm;

	bool IsValid() { return parseSuccess == S_OK; }
	void PrintOptions();

	OPMODE getOpMode() {
		int op = OPMODE_NONE;
		if (vm.count("mtiming")) op |= OPMODE_TIMINGS;
		if (vm.count("df")) op |= OPMODE_FIELDS;
		if (vm.count("mstats")) op |= OPMODE_STATS;

		return op;
	};

	Config* ProvideConfig() override;
	const wchar_t * HelpString() override;
private:
	CmdLine(CmdLine const&);        
	void operator=(CmdLine const&); 

	std::shared_ptr<Config> config;
	const wchar_t* helpString = L"";

	std::unique_ptr<po::options_description> desc;
	HRESULT parseSuccess;
	HRESULT Parse(int argc, wchar_t * argv[]);
	void FillConfig();
};