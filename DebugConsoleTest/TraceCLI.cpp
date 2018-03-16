#include "stdafx.h"
#include <map>
#include <stdio.h>
#include <conio.h>
#include <vector>
#include <iostream>
#include "..\Shared\Logger.h"
#include "..\Shared\IsElevated.h"
#include "..\DebugCore\Debugger.h"
#include "..\DebugCore\ProcessInfo.h"
#include "CmdLine.h"
#include "PredefinedConfigProviders.h"
#include "FileConfigReader.h"

using std::unique_ptr;

int _tmain(int argc, _TCHAR* argv[])
{
	auto cline = unique_ptr<CmdLine>(new CmdLine(argc, argv));
	if (argc < 2) return 0;

	if (cline->vm.count("help"))
	{
		return 0;
	}

	if (!IsElevated())
	{
		std::cout << std::endl << "Tip: Running elevated (as admin) can usually discover more processes" << std::endl;
	}

	if (cline->vm.count("processes"))
	{
		std::cout << "Listing .NET processes...";

		vector<shared_ptr<ProcInfo>> procInfo;
		auto processInfo = unique_ptr<ProcessInfo>(new ProcessInfo());
		if (processInfo->GetProcesses(true, procInfo) != S_OK)
		{
			std::cout << "error listing managed processes" << std::endl;
		}
		else
		{
			std::cout << procInfo.size() << " processes found." << std::endl;
			std::cout << std::endl;
			if (procInfo.size() == 0)
			{
				std::cout << "Try running as Admin to find more processes." << std::endl;
				return 0;
			}

			std::cout << "PID\t#CLR\tName" << std::endl;
			for (auto procIt = procInfo.begin(); procIt != procInfo.end(); ++procIt)
			{
				auto process = *procIt;
				auto procName = process->displayName;
				std::cout << process->pId << "\t(" << process->numRuntimes << ")\t";
				wprintf_s(procName);
				std::cout << std::endl;
			}
		}

		return 0;
	}

	if (cline->vm.count("runtimes"))
	{
		std::cout << "Listing installed .NET runtimes...";

		vector<shared_ptr<RuntimeInfo>> rInfo;
		auto processInfo = unique_ptr<ProcessInfo>(new ProcessInfo());
		if (processInfo->GetInstalledRuntimes(rInfo) != S_OK)
		{
			std::cout << "error listing runtimes" << std::endl;
		}
		else
		{
			std::cout << rInfo.size() << " runtimes found." << std::endl;
			std::cout << std::endl;
			std::cout << "Version\t\tFolder" << std::endl;
			for (auto rIt = rInfo.begin(); rIt != rInfo.end(); ++rIt)
			{
				auto runtime = *rIt;
				wprintf_s(runtime->versionString);
				wprintf_s(L"\t");
				wprintf_s(runtime->runtimeFolder);
				std::cout << std::endl;
			}
		}

		return 0;
	}

	//get config provider (default to command line if no others found)
	IConfigProvider* configProvider;
	if (cline->vm.count("pAD"))
	{
		configProvider = new ADConfigProvider();
	}	
	else if (cline->vm.count("pSQL"))
	{
		configProvider = new SQLConfigProvider();
	}	
#ifdef _DEBUG
	else
	if (cline->vm.count("pTEST"))
	{
		configProvider = new UnitTestConfigProvider();
	} 
#endif
	else
	if (cline->vm.count("config"))
	{
		auto confc = cline->vm["config"].as<std::string>();
		std::wstring confw;
		confw.assign(confc.begin(), confc.end());

		configProvider = new FileConfigReader(confw.c_str());		
	}
	else
	{
		configProvider = cline.get();
	}

	//get config
	auto config = configProvider->ProvideConfig();


	//setup logging
	Logger::CreateLog(config->OutfileName);
	//write header to log
	LOG(L"%S\n", VERSIONSTRING);
	//write config to log
	LOG(L"### PARAMETERS\n");
	auto filterNum = 0;
	for (auto filterIt = config->Breakpoints.begin(); filterIt != config->Breakpoints.end(); ++filterIt)
	{
		filterNum++;
		auto filt = *filterIt;
		LOG(L"Filter #%u: namespace %s, class %s, method %s.\n", filterNum, filt->namespaceFilter, filt->classFilter, filt->methodFilter);
		if (filt->fieldsToDump.size())
		{
			wchar_t fieldBuf[2048] = L"";
			for (auto dfIt = filt->fieldsToDump.begin(); dfIt != filt->fieldsToDump.end(); ++dfIt)
			{
				if (wcslen(fieldBuf)) wcscat_s(fieldBuf, 2048, L", ");

				wcscat_s(fieldBuf, 2048, *dfIt);
			}
			LOG(L"  Fields dumped on entry: %s\n", fieldBuf);
		}
	}

	//start actual attach
	if (cline->vm.count("attach"))
	{
		//check process exists, and is not us
		auto pId = cline->vm["attach"].as<int>();

		auto thispId = GetCurrentProcessId();
		if (thispId == pId)
		{
			std::cout << "Can't trace SELF" << std::endl;
			LOG(L"Can't trace SELF\n");
			return 0;
		}

		vector<shared_ptr<ProcInfo>> procInfo;
		auto processInfo = unique_ptr<ProcessInfo>(new ProcessInfo());
		if (processInfo->GetProcesses(true, procInfo) != S_OK)
		{
			std::cout << "Error querying processes, aborting" << std::endl;
			LOG(L"Error querying processes, aborting\n");
			return 0;
		}

		bool processFound = false;
		shared_ptr<ProcInfo> theProc = nullptr;
		for (auto pIt = procInfo.begin(); pIt != procInfo.end(); ++pIt)
		{
			auto proc = *pIt;
			if (proc->pId == pId)
			{
				processFound = true;
				theProc = proc;
			}
		}
		if (!processFound)
		{
			std::cout << "No managed process found with pId " << pId << std::endl;
			LOG(L"No managed process found with pId %u\n", pId);
			return 0;
		}

		LOG(L"### ATTACH\n");
		LOG(L"Attaching to process with pId %u: %s\n", pId, theProc->displayName);

		//establish instrumentation mode
		auto mode = config->OperatingMode;

		std::cout << "Trying to attach to process with pId " << pId << std::endl;

		try
		{
			unique_ptr<Debugger> debugger;
			try
			{
				debugger = unique_ptr<Debugger>(new Debugger(mode, pId));
			}
			catch (...)
			{
				TRACE(L"Failed to attach debugger, is a debugger attached ?, exiting\n");
				LOG(L"Failed to attach, is a debugger attached ? => exit\n");
				std::cout << "Failed to attach, is a debugger attached ? => exit" << std::endl;
				return 0;
			}

			vector<shared_ptr<MethodInfo>> methods;
			for (auto bpFilterIt = config->Breakpoints.begin(); bpFilterIt != config->Breakpoints.end(); ++bpFilterIt)
			{
				LOG(L"Processing filter...\n");
				auto thisFilter = *bpFilterIt;

				//check minimum filters
				if ((thisFilter->namespaceFilter && wcslen(thisFilter->namespaceFilter)) + (thisFilter->classFilter && wcslen(thisFilter->classFilter)) + (thisFilter->methodFilter && wcslen(thisFilter->methodFilter)) == 0)
				{
					//std::cout << "No real filter, this will lead to EXTREME load, skipping" << std::endl;
					LOG(L"No real filter, skipping due to excessive search results.\n");
					continue;
				}

				debugger->FindManagedMethods(
					thisFilter->namespaceFilter ? thisFilter->namespaceFilter : nullptr,
					thisFilter->classFilter ? thisFilter->classFilter : nullptr,
					thisFilter->methodFilter ? thisFilter->methodFilter : nullptr,
					thisFilter->fieldsToDump,
					methods);
			}

			wprintf_s(L"Found %u methods satisfying the filters\n", methods.size());
			LOG(L"Found %u methods satisfying the filters\n", methods.size());

			//set all breakpoints
			debugger->Stop();
			for (auto methodIt = methods.begin(); methodIt != methods.end(); ++methodIt)
			{
				LOG(L"Found method: %s\n", (*methodIt)->parsedSignature.get());

				if (mode & OPMODE_FIELDS || mode & OPMODE_TIMINGS)
				{
					debugger->SetBPAtEntry(*methodIt, []() -> void { /*do something here once we have our breakpoint to custom delegate mapping in place*/	return;	});
				}
				if (mode & OPMODE_TIMINGS)
				{
					LOG(L"%u exit breakpoints set\n", debugger->SetBPAtExit(*methodIt, nullptr));
				}

				if (mode & OPMODE_STATS)
				{
					TRACE(L"%u BOX breakpoints set\n", debugger->SetBPAtOpCode(*methodIt, nullptr, CEE_BOX));
					TRACE(L"%u UNBOX breakpoints set\n", debugger->SetBPAtOpCode(*methodIt, nullptr, CEE_UNBOX));
					TRACE(L"%u UNBOXANY breakpoints set\n", debugger->SetBPAtOpCode(*methodIt, nullptr, CEE_UNBOXANY));

					TRACE(L"%u NEWOBJ breakpoints set\n", debugger->SetBPAtOpCode(*methodIt, nullptr, CEE_NEWOBJ));
					TRACE(L"%u NEWARR breakpoints set\n", debugger->SetBPAtOpCode(*methodIt, nullptr, CEE_NEWARR));
				}
			}
			debugger->Continue();

			// Test mem stats.
			debugger->Stop();
			auto memInfo = unique_ptr<MemoryInfo>(debugger->GetMemoryInfo());
			COR_HEAPINFO heapInfo;
			if (memInfo->GetManagedHeapInfo(heapInfo) == S_OK)
			{
				LOG(L"GC: %u heaps, concurrent GC: %s, type: %s\n", heapInfo.numHeaps, heapInfo.concurrent ? L"YES" : L"NO", heapInfo.gcType == CorDebugGCType::CorDebugWorkstationGC ? L"Workstation" : L"Server");
			}
			vector<COR_SEGMENT> segments;
			if (memInfo->EnumerateManagedHeapSegments(segments) == S_OK)
			{
				LOG(L"GC heaps:\n");
				for (auto i : segments)
				{					
					const wchar_t* gentype = L"unknown";
					switch (i.type)
					{
						case CorDebugGenerationTypes::CorDebug_Gen0: gentype = L"Gen 0"; break;
						case CorDebugGenerationTypes::CorDebug_Gen1: gentype = L"Gen 1"; break;
						case CorDebugGenerationTypes::CorDebug_Gen2: gentype = L"Gen 2"; break;
						case CorDebugGenerationTypes::CorDebug_LOH: gentype = L"LOH"; break;			
					}
					LOG(L"Heap %u (%s): %llx=>%llx\n", i.heap, gentype, i.start, i.end);
				}
			}
			vector<HeapObjectStat> stats;
			if (memInfo->ManagedHeapStat(stats) == S_OK)
			{
				std::sort(stats.begin(), stats.end(), ByTotalSize());
				LOG(L"Objects on heap:\n");
				LOG(L"Num\tSize\tName\n");
				for (auto o : stats)
				{		
					TRACE(L"%llu\t%llu\t%s\n", o.count, o.size, o.name);
					LOG(L"%llu\t%llu\t%s\n", o.count, o.size, o.name);
				}
			}
			debugger->Continue();


			if (mode != OPMODE_NONE)
			{
				debugger->ActivateBPs(true);
				LOG(L"Activating all breakpoints\n");
			}


			LOG(L"### TRACING\n");

			std::cout << std::endl << "Press any key to stop tracing..." << std::endl;

			while (!_kbhit())
			{
			}

			LOG(L"Tracing stopped, finishing up.\n");
			LOG(L"### POSTPROCESSING\n");

			if (mode != OPMODE_NONE)
			{
				//get stats
				vector<shared_ptr<BreakpointInfo>> bpStats;
				debugger->GetBPStats(bpStats);

				if (mode & OPMODE_TIMINGS)
				{
					vector<shared_ptr<MethodInfo>> timedMethods;
					//get unique list of methods
					for (auto bpIt = bpStats.begin(); bpIt != bpStats.end(); ++bpIt)
					{
						auto bpMethod = (*bpIt)->method;

						if (bpMethod->methodEntered == 0) continue;

						if (std::find(timedMethods.begin(), timedMethods.end(), bpMethod) == timedMethods.end())
						{
							//not in the list -> add it
							timedMethods.push_back(bpMethod);
						}
					}

					//sort it by time spent
					std::sort(timedMethods.begin(), timedMethods.end(), ByTimeSpentPtr());

					//dump
					std::cout << "### Timing info" << std::endl;
					std::cout << "Hits\tMethod\tTotal (s)\tAvg (s)" << std::endl;
					LOG(L"## Timing info\n");
					LOG(L"Hits\tMethod\tTotal (s)\tAvg (s)\n");
					for (auto mIt = timedMethods.rbegin(); mIt != timedMethods.rend(); ++mIt)
					{
						auto met = *mIt;
						wprintf_s(L"%u\t%s\t%f\t%f\n", met->methodEntered, met->parsedSignature.get(), met->avgTimeInMethod * (double)met->methodEntered, met->avgTimeInMethod);
						LOG(L"%u\t%s\t%f\t%f\n", met->methodEntered, met->parsedSignature.get(), met->avgTimeInMethod * (double)met->methodEntered, met->avgTimeInMethod);
					}
					std::cout << std::endl;
				}

				if (mode & OPMODE_STATS)
				{
					//sort breakpoints by hitcount
					std::sort(bpStats.begin(), bpStats.end(), ByHitCountPtr());

					LOG(L"## Statistics\n");
					//dump boxing
					std::cout << "# BOX" << std::endl;
					std::cout << "Hits\tLocation\tType" << std::endl;
					LOG(L"# BOX\n");
					LOG(L"Hits\tLocation\tType\n");
					for (auto bpIt = bpStats.rbegin(); bpIt != bpStats.rend() && (*bpIt)->hitCount > 0; ++bpIt)
						if ((*bpIt)->CILInstruction == CEE_BOX)
						{
							wprintf_s(L"%u\t%s.0x%x\t%s\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
							LOG(L"%u\t%s.0x%x\t%s\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
						}
					std::cout << std::endl;

					//dump unboxing
					std::cout << "# UNBOX" << std::endl;
					std::cout << "Hits\tLocation\tType" << std::endl;
					LOG(L"# UNBOX\n");
					LOG(L"Hits\tLocation\tType\n");
					for (auto bpIt = bpStats.rbegin(); bpIt != bpStats.rend() && (*bpIt)->hitCount > 0; ++bpIt)
						if (CEE_UNBOXOPCODE((*bpIt)->CILInstruction))
						{
							wprintf_s(L"%u\t%s.0x%x\t%s\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
							LOG(L"%u\t%s.0x%x\t%s\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
						}
					std::cout << std::endl;

					//dump newobj
					std::cout << "# Allocations" << std::endl;
					std::cout << "Hits\tLocation\tCtor" << std::endl;
					LOG(L"# Allocations (new object)\n");
					LOG(L"Hits\tLocation\tConstructor\n");
					for (auto bpIt = bpStats.rbegin(); bpIt != bpStats.rend() && (*bpIt)->hitCount > 0; ++bpIt)
						if ((*bpIt)->CILInstruction == CEE_NEWOBJ)
						{
							wprintf_s(L"%u\t%s.0x%x\t%s\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
							LOG(L"%u\t%s.0x%x\t%s\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
						}
					std::cout << std::endl;

					//dump newarray
					std::cout << "# Array allocations" << std::endl;
					std::cout << "Hits\tLocation\tType" << std::endl;
					LOG(L"# Array allocations\n");
					LOG(L"Hits\tLocation\tElement type\n");
					for (auto bpIt = bpStats.rbegin(); bpIt != bpStats.rend() && (*bpIt)->hitCount > 0; ++bpIt)
						if ((*bpIt)->CILInstruction == CEE_NEWARR)
						{
							wprintf_s(L"%u\t%s.0x%x\t%s[]\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
							LOG(L"%u\t%s.0x%x\t%s[]\n", (*bpIt)->hitCount, (*bpIt)->method->parsedSignature.get(), (*bpIt)->ilOffset, debugger->GetName((*bpIt)->typeToken));
						}
					std::cout << std::endl;
				}
			}
		}
		catch (...)
		{
			std::cout << "unexpected error while attached, trying to do a clean shutdown" << std::endl;
			LOG(L"Unexpected error, shutting down\n");
		}

		return 0;
	}
	else
	{
		std::cout << "can't attach without -a parameter" << std::endl;
		return 0;
	}

	//no valid option
	cline->PrintOptions();

	return 0;
}