#include "SigParser.h"
#pragma once

struct HeapObjectStat
{
	ULONG64 size;
	COR_TYPEID type;
	ULONG64 count;
	wchar_t* name;
	
	//std vector sorting
	bool operator < (const HeapObjectStat& str) const
	{
		return (count < str.count);
	}
};

struct GCReference : COR_GC_REFERENCE
{
	wchar_t* typeName;
	wchar_t* address;
	wchar_t* value;
};

namespace std
{
	template<> struct less<COR_TYPEID>
	{
		bool operator() (const COR_TYPEID& lhs, const COR_TYPEID& rhs)
		{
			return 
				lhs.token1 != rhs.token1 
				? lhs.token1 < rhs.token1 
				: lhs.token2 < rhs.token2; 
		}
	};
}

class MemoryInfo
{
public:
	MemoryInfo(ICorDebugProcess *pProcess);
	HRESULT Init();
	HRESULT GetManagedHeapInfo(COR_HEAPINFO &info);
	HRESULT EnumerateManagedHeapSegments(vector<COR_SEGMENT> &segments);
	HRESULT ManagedHeapStat(vector<HeapObjectStat> &stats);
	HRESULT Handles(vector<GCReference> &handles);
	HRESULT GCRoots(vector<GCReference> &roots, bool includeWeakReferences, CorGCReferenceType filter);
	~MemoryInfo();
private:
	ComPtr<ICorDebugProcess> pProcess;
	ComPtr<ICorDebugProcess5> pProcess5;
	bool GCIsPossible();
};

//map sorting
struct ByTotalSize : public std::binary_function<HeapObjectStat, HeapObjectStat, bool>
{
	bool operator()(const HeapObjectStat& lhs, const HeapObjectStat& rhs) const
	{
		return lhs.count * lhs.size < rhs.count * rhs.size;
	}
};

