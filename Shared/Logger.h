#pragma once

#include <stdio.h>

struct Logger
{
	Logger();
	
	static bool CreateLog(const wchar_t* file);

#define maxLog 10240

	template<typename... Args>
	auto operator()(wchar_t const * format, Args... args) const -> void
	{
		wchar_t buffer[maxLog];

		SYSTEMTIME now;
		GetSystemTime(&now);

		auto count = swprintf_s(buffer, L"%02u:%02u:%02u.%03u ", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
		ASSERT(count != -1);		

		auto success = _snwprintf_s(buffer + count, maxLog - count, maxLog - count - 1, format, args...) != -1;

		if (_wfopen_s(&logFile, fileName, L"a") == 0)
		{
			fwprintf_s(logFile, buffer);
			if (!success) fwprintf_s(logFile, L"\nTRUNCATED after %u characters\n", maxLog);
			fclose(logFile);
		}
	}
private:
	static FILE* logFile;
	static const wchar_t* fileName;
};

#define LOG Logger()