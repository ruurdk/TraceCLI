#pragma once

#include <crtdbg.h>
#include <stdio.h>
#include <Windows.h>

#define ASSERT _ASSERTE 

#ifdef _DEBUG
#define VERIFY ASSERT
#else
#define VERIFY(expression) (expression)
#endif

struct Tracer
{
	const char* m_fileName;
	const unsigned int m_line;

#define maxTrace 204800

	Tracer(const char *fileName, const unsigned int line) : m_fileName(fileName), m_line(line) {}

	template<typename... Args>
	auto operator()(wchar_t const * format, Args... args) const -> void
	{			
		wchar_t buffer[maxTrace];

		auto count = swprintf_s(buffer, L"%S(%d): ", m_fileName, m_line);
		ASSERT(count != -1);

		//auto test = _countof(buffer);
		
		auto success = _snwprintf_s(buffer + count, maxTrace - count, maxTrace - count - 1, format, args...) != -1;

		OutputDebugString(buffer);
		if (!success) OutputDebugString(L"\nTRUNCATED\n");
	}
};

#ifdef _DEBUG
#define TRACE Tracer(__FILE__, __LINE__)
#else
#define TRACE __noop
#endif