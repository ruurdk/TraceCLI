#include "..\Shared\tracing.h"
#include "CmdLine.h"

#include "..\Shared\stdstringsplit.h"

CmdLine::CmdLine(int argc, wchar_t * argv[]) : parseSuccess(E_FAIL)
{
	ASSERT(argv);

	desc = std::unique_ptr<po::options_description>(new po::options_description("Options"));

	desc->add_options()
		("help,h", "print help messages")
		("processes,p", "print available .NET processes")
		("runtimes,r", "print installed .NET runtime (CLR) versions")
		("attach,a", po::value<int>(), "Attach to process with specified pId")
		("outfile,o", po::value<std::string>(), "output file (default: tracer.log)")
		("mtiming", "mode of operation: timing")
		("mstats", "mode of operation: deep statistics")
		("fn", po::value<std::string>(), "filter namespace")
		("fc", po::value<std::string>(), "filter fully qualified classname")
		("fm", po::value<std::string>(), "filter method")
		("df", po::value<std::string>(), "fields to dump - comma seperated")
		("pSQL", "preset filter: SQL trace (overrides commandline filters)")
		("pAD", "preset filter: AD trace (overrides commandline filters)")
		("config", po::value<std::string>(), "use config file for settings")
#ifdef _DEBUG
		("pTEST", "preset filter: Unit Test")
#endif
		;

	parseSuccess = Parse(argc, argv);
}

#define printOptions std::cout << *desc << std::endl

void CmdLine::PrintOptions()
{
	printOptions;
	std::cout << std::endl << "Example usage:" << std::endl;
	std::cout << "-dump .NET processes\n\t -p" << std::endl;
	std::cout << "-find and dump all ToString* methods in process with Id 1001\n\t -a 1001 --fm ToString" << std::endl;
	std::cout << "-select preset SQL trace filter and attach to process 1001\n\t -a 1001 --pSQL" << std::endl;
	std::cout << "-find and time all methods in namespace RuurdKeizer.* in process with Id 1001\n\t -a 1001 --fn RuurdKeizer. --mtiming" << std::endl;
	std::cout << "-find RunExecuteReader* methods in process 1001, and dump _commandText\n\t -a 1001 --fm RunExecuteReader --df _commandText" << std::endl;
}

HRESULT CmdLine::Parse(int argc, wchar_t * argv[])
{
	ASSERT(argv);

	try
	{
		std::cout << VERSIONSTRING << std::endl << std::endl;

		if (argc < 2)
		{
			PrintOptions();
			return S_OK;
		}

		po::store(po::parse_command_line(argc, argv, *desc), vm); // can throw 

		/** --help option
		*/
		if (vm.count("help"))
		{
			PrintOptions();
			return S_OK;
		}

		po::notify(vm);											// throws on error, so do after help in case there are any problems 

		FillConfig();
	}
	catch (po::error& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
		return E_FAIL;
	}

	return S_OK;
}

void CmdLine::FillConfig()
{
	auto retval = new Config{};
	retval->OperatingMode = getOpMode();

	if (vm.count("outfile"))
	{
		auto outfile = vm["outfile"].as<std::string>();
		std::wstring outw;
		outw.assign(outfile.begin(), outfile.end());
		auto coutf = outw.c_str();

		retval->OutfileName = new wchar_t[wcslen(coutf) + 1];
		wcscpy_s(retval->OutfileName, wcslen(coutf) + 1, coutf);
	}

	auto filter = new BPFilter{};

	if (vm.count("fn"))
	{
		auto fn = vm["fn"].as<std::string>();
		std::wstring fnw;
		fnw.assign(fn.begin(), fn.end());
		auto wfn = fnw.c_str();

		filter->namespaceFilter = new wchar_t[wcslen(wfn) + 1];
		wcscpy_s(filter->namespaceFilter, wcslen(wfn) + 1, wfn);
	}
	if (vm.count("fc"))
	{
		auto fc = vm["fc"].as<std::string>();
		std::wstring fcw;
		fcw.assign(fc.begin(), fc.end());
		auto wfc = fcw.c_str();

		filter->classFilter = new wchar_t[wcslen(wfc) + 1];
		wcscpy_s(filter->classFilter, wcslen(wfc) + 1, wfc);
	}
	if (vm.count("fm"))
	{
		auto fm = vm["fm"].as<std::string>();
		std::wstring fmw;
		fmw.assign(fm.begin(), fm.end());
		auto wfm = fmw.c_str();

		filter->methodFilter = new wchar_t[wcslen(wfm) + 1];
		wcscpy_s(filter->methodFilter, wcslen(wfm) + 1, wfm);
	}

	//parse fields to dump
	if (vm.count("df"))
	{
		auto df = vm["df"].as<std::string>();

		vector<std::string> parsedFields;
		split(df, ',', parsedFields);

		for (auto fieldIt = parsedFields.begin(); fieldIt != parsedFields.end(); ++fieldIt)
		{
			auto thisField = *fieldIt;
			//skip if empty
			if (!thisField.size()) continue;

			//convert to wstring
			std::wstring ws;
			ws.assign(thisField.begin(), thisField.end());

			//push on heap
			auto asCStr = ws.c_str();
			auto newHeapStr = new wchar_t[wcslen(asCStr) + 1];
			wcscpy_s(newHeapStr, wcslen(asCStr) + 1, asCStr);

			filter->fieldsToDump.push_back(newHeapStr);
		}
	}

	retval->Breakpoints.push_back(shared_ptr<BPFilter>(filter));

	config = shared_ptr<Config>(retval);
}

Config* CmdLine::ProvideConfig()
{
	return config.get();
}

const wchar_t* CmdLine::HelpString()
{
	return helpString;
}