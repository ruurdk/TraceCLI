#include "precompiled.h"
#include "MetaHelpers.h"
#include "SigParser.h"

MetaHelpers::MetaHelpers(ICorDebugProcess *pProcess)
{
	this->pProcess = pProcess;
}

MetaHelpers::~MetaHelpers()
{
}

wchar_t * MetaHelpers::GetName(const mdToken &token)
{
	auto findName = tokenCache.find(token);
	if (findName == tokenCache.end()) return nullptr;

	return findName->second.get();
}

void MetaHelpers::ResolveTokenAndAddToCache(mdToken token, ICorDebugFunction *enclosingMethod)
{
	// Find the token type.
	auto typeToken = TypeFromToken(token);

	switch (typeToken)
	{
		// Do we know how to resolve ?
		case CorTokenType::mdtMemberRef:
		case CorTokenType::mdtMethodDef:
		case CorTokenType::mdtTypeRef:
		case CorTokenType::mdtTypeDef:
		case CorTokenType::mdtFieldDef:
		{
			// Is it already in cache ?
			if (GetName(token) != nullptr)
				return;

			break;
		}
		default:
			return;
	}

	// Access metadata.
	ComPtr<ICorDebugModule> mod;
	ComPtr<IMetaDataImport> modmeta;
	if ((enclosingMethod->GetModule(&mod) != S_OK) || (mod->GetMetaDataInterface(IID_IMetaDataImport, &modmeta) != S_OK))
		return;

	// Depending on the token, get the name.
	shared_ptr<wchar_t> nameCopy;
	switch (typeToken)
	{
	case CorTokenType::mdtTypeDef:
		nameCopy = GetTypeDefName(token, modmeta.Get());
		break;
	case CorTokenType::mdtTypeRef:
		nameCopy = GetTypeRefName(token, modmeta.Get());
		break;
	case CorTokenType::mdtMemberRef:
		nameCopy = GetMemberRefName(token, modmeta.Get());
		break;
	case CorTokenType::mdtMethodDef:
		nameCopy = GetMethodDefName(token, modmeta.Get());
		break;
	case CorTokenType::mdtFieldDef:
		nameCopy = GetFieldDefName(token, modmeta.Get());
		break;
	default:
		return;
	}

	if (nameCopy == nullptr)
		return;

	AddToCache(token, nameCopy);
}


shared_ptr<wchar_t> MetaHelpers::GetTypeDefName(mdToken token, IMetaDataImport* modmeta)
{
	mdToken scopeToken;
	wchar_t name[1024];
	ULONG nameLen;
	DWORD typeDefFlags;

	if (modmeta->GetTypeDefProps(token, name, _countof(name), &nameLen, &typeDefFlags, &scopeToken) != S_OK)
		return nullptr;

	auto nameCopy = new wchar_t[nameLen + 1];
	VERIFY(wcscpy_s(nameCopy, nameLen + 1, name) == 0);

	return shared_ptr<wchar_t>(nameCopy);
}

shared_ptr<wchar_t> MetaHelpers::GetTypeRefName(mdToken token, IMetaDataImport* modmeta)
{
	mdToken scopeToken;
	wchar_t name[1024];
	ULONG nameLen;

	if (modmeta->GetTypeRefProps(token, &scopeToken, name, _countof(name), &nameLen) != S_OK)
		return nullptr;

	// Cache the type name.
	auto nameCopy = new wchar_t[nameLen + 1];
	VERIFY(wcscpy_s(nameCopy, nameLen + 1, name) == 0);

	return shared_ptr<wchar_t>(nameCopy);
}

shared_ptr<wchar_t> MetaHelpers::GetMemberRefName(mdToken token, IMetaDataImport* modmeta)
{
	mdToken enclosingTypeToken;
	wchar_t memberName[1024];
	ULONG memberNameLen;
	PCCOR_SIGNATURE memberSig;
	ULONG memberSigBytes;
	if (modmeta->GetMemberRefProps(token, &enclosingTypeToken, memberName, _countof(memberName), &memberNameLen, &memberSig, &memberSigBytes) != S_OK)
		return nullptr;

	// get the type as well
	mdToken scopeToken;
	wchar_t enclosingTypeName[1024];
	ULONG enclosingTypeNameLen;
	if (modmeta->GetTypeRefProps(enclosingTypeToken, &scopeToken, enclosingTypeName, _countof(enclosingTypeName), &enclosingTypeNameLen) != S_OK)
		return nullptr;

	auto sigP = unique_ptr<SigParser>(new SigParser(enclosingTypeName, memberSig, memberSigBytes, memberName, modmeta, token, 0, 0));

	auto sig = sigP->Signature();
	auto sigLen = wcslen(sig);
	auto nameCopy = new wchar_t[sigLen + 1];
	VERIFY(wcscpy_s(nameCopy, sigLen + 1, sig) == 0);

	return shared_ptr<wchar_t>(nameCopy);
}

shared_ptr<wchar_t> MetaHelpers::GetMethodDefName(mdToken token, IMetaDataImport* modmeta)
{
	mdToken enclosingTypeToken;
	wchar_t memberName[1024];
	ULONG memberNameLen;
	PCCOR_SIGNATURE memberSig;
	ULONG memberSigBytes;

	DWORD pAttr;
	ULONG codeRVA;
	DWORD implFlags;
	DWORD constType;
	UVCP_CONSTANT constVal;
	ULONG constBytes;
	if (modmeta->GetMemberProps(token, &enclosingTypeToken, memberName, _countof(memberName), &memberNameLen, &pAttr, &memberSig, &memberSigBytes, &codeRVA, &implFlags, &constType, &constVal, &constBytes) != S_OK)
		return nullptr;

	// get the type as well
	mdToken scopeToken;
	wchar_t enclosingTypeName[1024];
	ULONG enclosingTypeNameLen;

	DWORD typeDefFlags;
	if (modmeta->GetTypeDefProps(enclosingTypeToken, enclosingTypeName, _countof(enclosingTypeName), &enclosingTypeNameLen, &typeDefFlags, &scopeToken) != S_OK)
		return nullptr;

	auto sigP = unique_ptr<SigParser>(new SigParser(enclosingTypeName, memberSig, memberSigBytes, memberName, modmeta, token, 0, 0));

	auto sig = sigP->Signature();
	auto sigLen = wcslen(sig);
	auto nameCopy = new wchar_t[sigLen + 1];
	VERIFY(wcscpy_s(nameCopy, sigLen + 1, sig) == 0);

	return shared_ptr<wchar_t>(nameCopy);
}

shared_ptr<wchar_t> MetaHelpers::GetFieldDefName(mdToken token, IMetaDataImport* modmeta)
{
	mdToken enclosingTypeToken;
	wchar_t memberName[1024];
	ULONG memberNameLen;
	PCCOR_SIGNATURE memberSig;
	ULONG memberSigBytes;

	DWORD pAttr;
	DWORD constType;
	UVCP_CONSTANT constVal;
	ULONG constBytes;
	if (modmeta->GetFieldProps(token, &enclosingTypeToken, memberName, _countof(memberName), &memberNameLen, &pAttr, &memberSig, &memberSigBytes, &constType, &constVal, &constBytes) != S_OK)
		return nullptr;

	// get the type as well
	mdToken scopeToken;
	wchar_t enclosingTypeName[1024];
	ULONG enclosingTypeNameLen;

	DWORD typeDefFlags;
	if (modmeta->GetTypeDefProps(enclosingTypeToken, enclosingTypeName, _countof(enclosingTypeName), &enclosingTypeNameLen, &typeDefFlags, &scopeToken) != S_OK)
		return nullptr;

	auto sigP = unique_ptr<SigParser>(new SigParser(enclosingTypeName, memberSig, memberSigBytes, memberName, modmeta, token, 0, 0));

	auto sig = sigP->Signature();
	auto sigLen = wcslen(sig);
	auto nameCopy = new wchar_t[sigLen + 1];
	VERIFY(wcscpy_s(nameCopy, sigLen + 1, sig) == 0);

	return shared_ptr<wchar_t>(nameCopy);
}