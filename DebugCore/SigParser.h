#include "precompiled.h"

#pragma once
class SigParser
{
public:
	SigParser(const wchar_t *className, const PCCOR_SIGNATURE sigBytes, ULONG sigSize, const wchar_t *name, IMetaDataImport * const pMetaData, mdToken token, DWORD implFlags, DWORD attrFlags);	
	const wchar_t *Signature() const;
private:
	HRESULT Parse();
	HRESULT ParseMethod();
	HRESULT ParseField();
	
	ULONG AddType(PCCOR_SIGNATURE sigBlob);
	void AddString(const wchar_t *str);

	PCCOR_SIGNATURE sigBytes;
	ULONG sigSize;
	const wchar_t *name;
	const wchar_t *className;
	ComPtr<IMetaDataImport> metaData;
	mdToken token;
	DWORD implFlags;
	DWORD attrFlags;

	int completeNameLen;
	unique_ptr<wchar_t[]> completeName;

	unique_ptr<wchar_t[]> parsedSignature;
};

