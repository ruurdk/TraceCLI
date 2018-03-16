#include "precompiled.h"

#pragma once
struct IDebugger
{
	virtual void OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &Breakpoint) = 0;
	virtual void OnException(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags) = 0;
};