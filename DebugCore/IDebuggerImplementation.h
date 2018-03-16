#pragma once
struct IDebuggerImplementation
{
	virtual ICorDebugProcess* const CorProcess(void) = 0;
	virtual ICorDebugProcess5* const CorProcess5(void) = 0;
	virtual void OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &Breakpoint) = 0;
	virtual void OnException(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags) = 0;
	virtual ~IDebuggerImplementation() {};
};