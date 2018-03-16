#include "PredefinedConfigProviders.h"

Config* SQLConfigProvider::ProvideConfig()
{
	auto returnval = new Config{};
	returnval->OperatingMode = OPMODE_FIELDS;

	auto filter = shared_ptr<BPFilter>(new BPFilter{});
	filter->methodFilter = L"RunExecuteReader";
	filter->fieldsToDump.push_back(L"_commandText");
	returnval->Breakpoints.push_back(filter);

	auto filter2 = shared_ptr<BPFilter>(new BPFilter{});
	filter2->methodFilter = L"RunExecuteScalar";
	filter2->fieldsToDump.push_back(L"_commandText");
	returnval->Breakpoints.push_back(filter2);

	auto filter3 = shared_ptr<BPFilter>(new BPFilter{});
	filter3->methodFilter = L"RunExecuteNonQuery";
	filter3->fieldsToDump.push_back(L"_commandText");
	returnval->Breakpoints.push_back(filter3);

	return returnval;
}

const wchar_t* SQLConfigProvider::HelpString()
{
	return nullptr;
}

Config* ADConfigProvider::ProvideConfig()
{
	auto returnval = new Config{};
	returnval->OperatingMode = OPMODE_FIELDS;

	auto filter = shared_ptr<BPFilter>(new BPFilter{});
	filter->classFilter = L"System.DirectoryServices.DirectorySearcher";
	filter->methodFilter = L"ExecuteSearch";
	filter->fieldsToDump.push_back(L"filter");
	returnval->Breakpoints.push_back(filter);

	auto filter2 = shared_ptr<BPFilter>(new BPFilter{});
	filter2->classFilter = L"System.DirectoryServices.DirectoryEntry";
	filter2->fieldsToDump.push_back(L"path");
	returnval->Breakpoints.push_back(filter2);

	return returnval;
}

const wchar_t* ADConfigProvider::HelpString()
{
	return nullptr;
}

Config* UnitTestConfigProvider::ProvideConfig()
{
	auto returnval = new Config{};
	returnval->OperatingMode = OPMODE_FIELDS | OPMODE_STATS | OPMODE_TIMINGS;

	auto filter = shared_ptr<BPFilter>(new BPFilter{});
	filter->classFilter = L"DebugTestTarget.InstanceTest";
	filter->methodFilter = L"SleepOne";

	//changing field check
	filter->fieldsToDump.push_back(L"sleepCount");
	//types of fields check
	filter->fieldsToDump.push_back(L"testStringStatic");
	//the COR_ELEMENT_TYPES (instances)
	filter->fieldsToDump.push_back(L"testUByte");
	filter->fieldsToDump.push_back(L"testUShort");
	filter->fieldsToDump.push_back(L"testUInt");
	filter->fieldsToDump.push_back(L"testULong");
	filter->fieldsToDump.push_back(L"testByte");
	filter->fieldsToDump.push_back(L"testShort");
	filter->fieldsToDump.push_back(L"testInt");
	filter->fieldsToDump.push_back(L"testLong");
	filter->fieldsToDump.push_back(L"testIntPtr");
	filter->fieldsToDump.push_back(L"testUIntPtr");
	filter->fieldsToDump.push_back(L"testChar");
	filter->fieldsToDump.push_back(L"testStringInstance");
	filter->fieldsToDump.push_back(L"testBool");
	filter->fieldsToDump.push_back(L"testFloat");
	filter->fieldsToDump.push_back(L"testDouble");

	filter->fieldsToDump.push_back(L"testPtr");
	filter->fieldsToDump.push_back(L"testValueType");
	filter->fieldsToDump.push_back(L"testNestedClass");

	filter->fieldsToDump.push_back(L"testSZArray");
	filter->fieldsToDump.push_back(L"testMDArray");

	filter->fieldsToDump.push_back(L"testFuncPtr");
	filter->fieldsToDump.push_back(L"testObject");
	filter->fieldsToDump.push_back(L"testGeneric");

	returnval->Breakpoints.push_back(filter);

	return returnval;
}

const wchar_t* UnitTestConfigProvider::HelpString()
{
	return nullptr;
}