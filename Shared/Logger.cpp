#include "Logger.h"
#include "tracing.h"

FILE* Logger::logFile;
const wchar_t* Logger::fileName = L"tracer.log";

Logger::Logger() {}

bool Logger::CreateLog(const wchar_t* file)
{
	if (file != nullptr) fileName = file;

	//create it
	if (_wfopen_s(&logFile, fileName, L"w+") == 0)
	{
		VERIFY(fclose(logFile) == 0);
		return true;
	}
	return false;
}