#include "precompiled.h"
#include "MemoryInfo.h"
#include <algorithm>

using std::wstring;

MemoryInfo::MemoryInfo(ICorDebugProcess *pProcess)
{
	ASSERT(pProcess);

	this->pProcess = pProcess;
}

HRESULT MemoryInfo::Init()
{
	return this->pProcess.As(&pProcess5);
}

MemoryInfo::~MemoryInfo()
{
}

HRESULT MemoryInfo::GetManagedHeapInfo(COR_HEAPINFO &info)
{	
	if (!pProcess5) return E_FAIL;

	return pProcess5->GetGCHeapInformation(&info);
}

bool MemoryInfo::GCIsPossible()
{
	COR_HEAPINFO info;
	if (GetManagedHeapInfo(info) != S_OK) return false;

	return info.areGCStructuresValid == 1;
}

HRESULT MemoryInfo::EnumerateManagedHeapSegments(vector<COR_SEGMENT> &segments)
{	
	if (!pProcess5) return E_FAIL;
	if (!GCIsPossible()) return E_FAIL;
	
	ComPtr<ICorDebugHeapSegmentEnum> heapSegments;
	if (auto hr = pProcess5->EnumerateHeapRegions(&heapSegments) != S_OK) return hr;
	
	segments.clear();

	ULONG fetched;
	COR_SEGMENT segment;
	for (; heapSegments->Next(1, &segment, &fetched) == S_OK && fetched == 1;)
	{
		segments.push_back(segment);
	}

	return S_OK;
}

HRESULT MemoryInfo::Handles(vector<GCReference> &handles)
{
	if (!pProcess5) return E_FAIL;
	if (!GCIsPossible()) return E_FAIL;

	ComPtr<ICorDebugGCReferenceEnum> gcRefEnum;
	if (auto hr = pProcess5->EnumerateHandles(CorGCReferenceType::CorHandleAll, &gcRefEnum) != S_OK) return hr;

	handles.clear();

	GCReference ref;
	ULONG fetched;
	for (; gcRefEnum->Next(1, &ref, &fetched) == S_OK && fetched == 1;)
	{
		// todo: enrich structure with typename, address, value
		handles.push_back(ref);
	}

	return S_OK;
}

HRESULT MemoryInfo::GCRoots(vector<GCReference> &roots, bool includeWeakReferences, CorGCReferenceType filter)
{
	if (!pProcess5) return E_FAIL;
	if (!GCIsPossible()) return E_FAIL;

	ComPtr<ICorDebugGCReferenceEnum> gcRefEnum;
	if (auto hr = pProcess5->EnumerateGCReferences(includeWeakReferences, &gcRefEnum) != S_OK) return hr;

	roots.clear();

	GCReference ref;
	ULONG fetched;
	for (; gcRefEnum->Next(1, &ref, &fetched) == S_OK && fetched == 1;)
	{
		if ((ref.Type & filter) != 0)
		{
			// todo: enrich structure with typename, address, value
			roots.push_back(ref);
		}
	}

	return S_OK;
}

// todo redo this by leveraging MetaHelpers.cpp
void ResolveTypeName(ICorDebugType* type, wstring &name)
{	
	ASSERT(type);

	ComPtr<ICorDebugClass> corClass;
	mdTypeDef classToken;
	ComPtr<ICorDebugModule> corModule;
	ComPtr<IMetaDataImport> modMeta;
	wchar_t className[3000];
	ULONG classNameLen;
	DWORD dwTypeDefFlags;
	mdToken baseType;

	CorElementType eType;
	if (type->GetType(&eType) != S_OK)
	{
		name += wstring(L"<unknown cor element type>");
		return;
	}
	if (eType != CorElementType::ELEMENT_TYPE_CLASS && eType != CorElementType::ELEMENT_TYPE_VALUETYPE)
	{
		switch (eType)
		{
			case ELEMENT_TYPE_VOID:     name += wstring(L"System.Void"); break;
			case ELEMENT_TYPE_BOOLEAN:  name += wstring(L"System.Boolean"); break;
			case ELEMENT_TYPE_I1:       name += wstring(L"System.SByte"); break;
			case ELEMENT_TYPE_U1:       name += wstring(L"System.Byte"); break;
			case ELEMENT_TYPE_I2:       name += wstring(L"System.Int16"); break;
			case ELEMENT_TYPE_U2:       name += wstring(L"System.UInt16"); break;
			case ELEMENT_TYPE_CHAR:     name += wstring(L"System.Char"); break;
			case ELEMENT_TYPE_I4:       name += wstring(L"System.Int32"); break;
			case ELEMENT_TYPE_U4:       name += wstring(L"System.UInt32"); break;
			case ELEMENT_TYPE_I8:       name += wstring(L"System.Int64"); break;
			case ELEMENT_TYPE_U8:       name += wstring(L"System.UInt64"); break;
			case ELEMENT_TYPE_R4:       name += wstring(L"System.Single"); break;
			case ELEMENT_TYPE_R8:       name += wstring(L"System.Double"); break;
			case ELEMENT_TYPE_OBJECT:   name += wstring(L"System.Object"); break;
			case ELEMENT_TYPE_STRING:   name += wstring(L"System.String"); break;
			case ELEMENT_TYPE_I:        name += wstring(L"System.IntPtr"); break;
			case ELEMENT_TYPE_U:        name += wstring(L"System.UIntPtr"); break;
			case ELEMENT_TYPE_TYPEDBYREF: name += wstring(L"refany "); break;
			case ELEMENT_TYPE_PTR:
			case ELEMENT_TYPE_BYREF:
			{
				name += eType == ELEMENT_TYPE_PTR ? wstring(L"* ") : wstring(L"ref ");

				ComPtr<ICorDebugTypeEnum> corTypeParamEnum;
				ULONG typeParamCount;
				if ((type->EnumerateTypeParameters(&corTypeParamEnum) != S_OK)
					|| (corTypeParamEnum->GetCount(&typeParamCount) != S_OK)
					|| typeParamCount != 1) return;

				ComPtr<ICorDebugType> typeArg;
				ULONG typeArgsFetched;				
				if ((corTypeParamEnum->Next(1, &typeArg, &typeArgsFetched) == S_OK) && (typeArgsFetched == 1))
				{
					ResolveTypeName(typeArg.Get(), name);
				}
				break;
			}
			case ELEMENT_TYPE_SZARRAY:
			case ELEMENT_TYPE_ARRAY:
			{				
				ComPtr<ICorDebugTypeEnum> corTypeParamEnum;
				ULONG typeParamCount;
				if ((type->EnumerateTypeParameters(&corTypeParamEnum) != S_OK)
					|| (corTypeParamEnum->GetCount(&typeParamCount) != S_OK)
					|| typeParamCount != 1) return;
								
				ComPtr<ICorDebugType> typeArg;
				ULONG typeArgsFetched;	
				if ((corTypeParamEnum->Next(1, &typeArg, &typeArgsFetched) == S_OK) && (typeArgsFetched == 1))
				{
					ResolveTypeName(typeArg.Get(), name);					
				}			
				name += wstring(L"[");
				if (eType == ELEMENT_TYPE_ARRAY)
				{
					ULONG32 rank;
					if (type->GetRank(&rank) == S_OK)
					{
						for (ULONG32 ri = 1; ri < rank; ri++)
						{
							name += wstring(L",");							
						}
					}
				}
				name += wstring(L"]");
				break;
			}						
			case ELEMENT_TYPE_CMOD_REQD:name += wstring(L"CMOD_REQD"); break;
			case ELEMENT_TYPE_CMOD_OPT:	name += wstring(L"CMOD_OPT"); break;
			case ELEMENT_TYPE_PINNED:	name += wstring(L"pinned"); break;
			default:
				name += wstring(L"unimplemented simple type"); break;
		}
		return;
	}


	// First get the classname.
	if ((type->GetClass(&corClass) != S_OK)
		|| (corClass->GetToken(&classToken) != S_OK)
		|| (corClass->GetModule(&corModule) != S_OK)
		|| (corModule->GetMetaDataInterface(IID_IMetaDataImport, &modMeta) != S_OK)
		|| (modMeta->GetTypeDefProps(classToken, className, sizeof(className), &classNameLen, &dwTypeDefFlags, &baseType) != S_OK))
	{				
		name += wstring(L"unable to get class");
		return;		
	}

	// Is it nested ?
	mdTypeDef parentToken;
	wchar_t parentClassName[3000];
	ULONG parentClassNameLen;
	DWORD dwParentTypeDefFlags;
	mdToken parentBaseType;
	if (IsTdNested(dwTypeDefFlags) 
		&& (modMeta->GetNestedClassProps(classToken, &parentToken) == S_OK)
		&& (modMeta->GetTypeDefProps(parentToken, parentClassName, sizeof(parentClassName), &parentClassNameLen, &dwParentTypeDefFlags, &parentBaseType) == S_OK))
	{
		name += wstring(parentClassName);
		name += wstring(L".");
	}

	name += wstring(className);

	// And any possible type parameters.
	ComPtr<ICorDebugTypeEnum> corTypeParamEnum;
	ULONG typeParamCount;
	if ((type->EnumerateTypeParameters(&corTypeParamEnum) != S_OK)
		|| (corTypeParamEnum->GetCount(&typeParamCount) != S_OK)
		|| typeParamCount == 0) return;
	
	// If we got here, create type parameters

	name += wstring(L"<");
	ComPtr<ICorDebugType> typeArg;
	ULONG typeArgsFetched;
	ULONG arg = 0;
	for (; (corTypeParamEnum->Next(1, &typeArg, &typeArgsFetched) == S_OK) && (typeArgsFetched > 0);)
	{
		if (arg != 0) name += wstring(L",");
		arg++;

		ResolveTypeName(typeArg.Get(), name);
	}
	name += wstring(L">");
}

HRESULT MemoryInfo::ManagedHeapStat(vector<HeapObjectStat> &stats)
{	
	if (!pProcess5) return E_FAIL;
	if (!GCIsPossible()) return E_FAIL;

	HRESULT hr;

	// Is the heap enumerable?
	ComPtr<ICorDebugHeapEnum> heapEnum;
	if (hr = pProcess5->EnumerateHeap(&heapEnum) != S_OK) return hr;

	stats.clear();

	// Get all objects.
	ULONG objectCount = 0;
	ULONG togetatonce = 10000;
	ULONG numObjectsFetched;
	COR_HEAPOBJECT objectsFetched[10000];
	vector<COR_HEAPOBJECT> objects;
	do 
	{
		hr = heapEnum->Next(togetatonce, &objectsFetched[0], &numObjectsFetched);
		objectCount += numObjectsFetched;
		for (ULONG i = 0; i < numObjectsFetched; i++)
		{
			objects.push_back(objectsFetched[i]);
		}
	} while (hr == S_OK && numObjectsFetched == togetatonce);

	// Calculate stats (group into bins).
	map<COR_TYPEID, HeapObjectStat> tempMap; //todo sort on count automatically
	for (ULONG oIndex = 0; oIndex < objectCount; oIndex++)
	{
		auto currentObject = objects[oIndex];
		//auto inMap = tempMap.find(currentObject.type);
		auto inMap = std::find_if(tempMap.begin(), tempMap.end(), 
			[&currentObject](const std::pair<COR_TYPEID, HeapObjectStat> &entry)
			{
				return (entry.first.token1 == currentObject.type.token1) && (entry.first.token2 == currentObject.type.token2) && (entry.second.size == currentObject.size);
			});
		if (inMap != tempMap.end()) 
		{
			inMap->second.count++;
		}
		else 
		{
			HeapObjectStat newStat{};
			newStat.count = 1;
			newStat.size = currentObject.size;
			newStat.type = currentObject.type;
			tempMap[currentObject.type] = newStat;
		}
	}

	for (auto stat : tempMap)
	{
		// Convert COR_TYPE_ID to ICorDebugType.
		ComPtr<ICorDebugType> corType;
		if (pProcess5->GetTypeForTypeID(stat.first, &corType) != S_OK) continue;

		// Get the name of the instantiated type.
		wstring name;
		ResolveTypeName(corType.Get(), name);

		// Insert the name.	
		auto len = name.size();
		stat.second.name = new wchar_t[len + 1];
		VERIFY(wcscpy_s(stat.second.name, len + 1, name.c_str()) == 0);
		
		// Push in result.
		stats.push_back(stat.second);
	}

	return S_OK;
}
