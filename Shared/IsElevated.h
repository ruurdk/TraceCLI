#include <WinBase.h>
#include "tracing.h"

#pragma once

bool IsElevated()
{
	//open process token
	HANDLE hToken;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		return FALSE;
	}

	int tokenInformation;
	DWORD dwSize = sizeof(int);
	if (!GetTokenInformation(hToken, TokenElevation, &tokenInformation, dwSize, &dwSize))
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
		{
			VERIFY(CloseHandle(hToken));
			return FALSE;
		}
	}

	VERIFY(CloseHandle(hToken));

	return (tokenInformation != 0);
}