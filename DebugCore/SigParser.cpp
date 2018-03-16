#include "precompiled.h"
#include "SigParser.h"

SigParser::SigParser(const wchar_t *className, const PCCOR_SIGNATURE sigBytes, ULONG sigSize, const wchar_t *name, IMetaDataImport * const MetaData, mdToken token, DWORD implFlags, DWORD attrFlags) : metaData(nullptr)
{
	this->sigBytes = sigBytes;
	this->sigSize = sigSize;
	this->name = name;
	this->metaData = MetaData;
	this->token = token;
	this->className = className;

	this->implFlags = implFlags;
	this->attrFlags = attrFlags;

	completeNameLen = 3000;
	completeName = unique_ptr<wchar_t[]>(new wchar_t[completeNameLen]);
	completeName[0] = '\0';

	if (Parse() == S_OK)
	{
		auto size = wcslen(completeName.get());
		parsedSignature = unique_ptr<wchar_t[]>(new wchar_t[size + 1]);
		VERIFY(wcscpy_s(parsedSignature.get(), size + 1, completeName.get()) == 0);
	}
	else
	{
		parsedSignature = unique_ptr<wchar_t[]>(const_cast<wchar_t*>(L"invalid signature"));
	}

	completeName = nullptr;
}

const wchar_t * SigParser::Signature() const
{
	return parsedSignature.get();
}

HRESULT SigParser::Parse()
{
	PCCOR_SIGNATURE sigIt = sigBytes;

	//bitflags
	ULONG bitFlags;
	CorSigUncompressData(sigIt, &bitFlags);

	if (bitFlags & IMAGE_CEE_CS_CALLCONV_FIELD)
	{
		return ParseField();
	}
	else
	{
		return ParseMethod();
	}
}

HRESULT SigParser::ParseMethod()
{
	PCCOR_SIGNATURE sigIt = sigBytes;

	//1 - first parse the attributes
	if (IsMdPrivate(attrFlags)) AddString(L"private ");
	if (IsMdPublic(attrFlags)) AddString(L"public ");
	if (IsMdFamily(attrFlags)) AddString(L"protected ");
	if (IsMdAssem(attrFlags)) AddString(L"internal ");
	if (IsMdFamANDAssem(attrFlags)) AddString(L"protected internal ");
	if (IsMdFamORAssem(attrFlags)) AddString(L"protected OR internal ");

	if (IsMdFinal(attrFlags)) AddString(L"final ");
	if (IsMdVirtual(attrFlags)) AddString(L"virtual ");
	//if (IsMdHideBySig(attrFlags)) AddString(L"hidebysig "); //seems default for C# but not for CLR

	//if (IsMdReuseSlot(attrFlags)) AddString(L"reuseslot ");
	if (IsMdNewSlot(attrFlags)) AddString(L"newslot ");

	if (IsMdCheckAccessOnOverride(attrFlags)) AddString(L"accessonoverride ");
	if (IsMdAbstract(attrFlags)) AddString(L"abstract ");
	//if (IsMdSpecialName(attrFlags)) AddString(L"specialname ");

	if (IsMdPinvokeImpl(attrFlags)) AddString(L"pinvoke ");
	if (IsMdUnmanagedExport(attrFlags)) AddString(L"unmanagedexport ");

	//if (IsMdRTSpecialName(attrFlags)) AddString(L"rtspecialname ");

	if (IsMdHasSecurity(attrFlags)) AddString(L"hassecurity ");
	if (IsMdRequireSecObject(attrFlags)) AddString(L"requiresecobject ");

	//2 - next the signature
	//bitflags
	ULONG bitFlags;
	sigIt += CorSigUncompressData(sigIt, &bitFlags);

	//if (bitFlags & IMAGE_CEE_CS_CALLCONV_HASTHIS) AddString(L"instance "); //instance is assumed default, removes clutter
	if ((bitFlags & (IMAGE_CEE_CS_CALLCONV_HASTHIS | IMAGE_CEE_CS_CALLCONV_EXPLICITTHIS)) == 0) AddString(L"static ");
	if (bitFlags & IMAGE_CEE_CS_CALLCONV_EXPLICITTHIS) AddString(L" explicitthis ");
	//if (bitFlags & IMAGE_CEE_CS_CALLCONV_DEFAULT) AddString(L" default");

	//cache the method name temporarily so we can put the return type before the method name
	wchar_t cachedNameAndArguments[2048] = L"\0";
	VERIFY(swprintf_s(cachedNameAndArguments, _countof(cachedNameAndArguments), L" %s::%s", className ? className : L"", name) != -1);

	//generic params ?
	int genericArguments = 0;
	if (bitFlags & IMAGE_CEE_CS_CALLCONV_GENERIC)
	{
		genericArguments = CorSigUncompressData(sigIt);
	}

	//number of parameters
	ULONG paramCount = CorSigUncompressData(sigIt);

	//return type
	auto sigItInc = AddType(sigIt);

	//now add the methodname
	AddString(cachedNameAndArguments);

	//add generic arguments
	if (genericArguments != 0)
	{
		AddString(L"<");

		//do something extra to resolve (from metadata, its not in the sig) the generic argument name, constraints, variance, etc
		ComPtr<IMetaDataImport2> meta2;
		HCORENUM hEnum = nullptr;
		mdGenericParam genericParams[100];
		ULONG numGenReturned;
		if ((metaData.CopyTo(IID_IMetaDataImport2, &meta2) == S_OK) && 
			(meta2->EnumGenericParams(&hEnum, token, genericParams, _countof(genericParams), &numGenReturned) == S_OK))
		{
			ULONG paramSeq;
			DWORD paramFlags;
			mdToken ownerTokenWeAlreadyKnow;
			DWORD reserved;
			wchar_t paramName[1024];
			ULONG paramNameLen;
			for (ULONG genPIt = 0; genPIt < numGenReturned; genPIt++)
			{
				if (genPIt != 0) AddString(L", ");
				if (meta2->GetGenericParamProps(genericParams[genPIt], &paramSeq, &paramFlags, &ownerTokenWeAlreadyKnow, &reserved, paramName, _countof(paramName), &paramNameLen) == S_OK)					
				{
					//variance
					if (paramFlags & CorGenericParamAttr::gpContravariant) AddString(L"in ");
					if (paramFlags & CorGenericParamAttr::gpCovariant) AddString(L"out ");

					AddString(paramName);

					//any constraints (cache this later to append to whole signature)
					if (paramFlags & CorGenericParamAttr::gpSpecialConstraintMask) 
					{
						AddString(L":");
						if (paramFlags & CorGenericParamAttr::gpReferenceTypeConstraint) AddString(L" class");
						if (paramFlags & CorGenericParamAttr::gpNotNullableValueTypeConstraint) AddString(L" struct");						
						if (paramFlags & CorGenericParamAttr::gpDefaultConstructorConstraint) AddString(L" new()");
					}
				}
			}
		}

		AddString(L">");
	}

	AddString(L"(");
	//param types
	for (ULONG paramIt = 0; paramIt < paramCount; paramIt++)
	{
		if (paramIt != 0) AddString(L", ");
		AddType(sigIt);
	}
	AddString(L") ");

	//3 - finally the implementation
	if (IsMiIL(implFlags)) AddString(L"cil ");
	if (IsMiNative(implFlags)) AddString(L"native ");
	if (IsMiOPTIL(implFlags)) AddString(L"optIL ");
	if (IsMiRuntime(implFlags)) AddString(L"runtime ");

	if (IsMiManaged(implFlags)) AddString(L"managed "); else AddString(L"unmanaged ");

	if (IsMiForwardRef(implFlags)) AddString(L"forwardref ");
	if (IsMiPreserveSig(implFlags)) AddString(L"preservesig ");

	if (IsMiInternalCall(implFlags)) AddString(L"internalcall ");

	if (IsMiSynchronized(implFlags)) AddString(L"synchronized ");
	if (IsMiNoInlining(implFlags)) AddString(L"noinlining ");
	if (IsMiAggressiveInlining(implFlags)) AddString(L"aggressiveinlining ");
	if (IsMiNoOptimization(implFlags)) AddString(L"nooptimization ");

	return S_OK;
}

HRESULT SigParser::ParseField()
{
	PCCOR_SIGNATURE sigIt = sigBytes;

	//bitflags
	ULONG bitFlags;
	sigIt += CorSigUncompressData(sigIt, &bitFlags);
	//check if the field is actually a field...
	if ((bitFlags & IMAGE_CEE_CS_CALLCONV_FIELD) == 0) return E_FAIL;


	//1 - first parse the attributes
	if (IsFdPrivate(attrFlags)) AddString(L"private ");
	if (IsFdPublic(attrFlags)) AddString(L"public ");
	if (IsFdFamily(attrFlags)) AddString(L"protected ");
	if (IsFdAssembly(attrFlags)) AddString(L"internal ");
	if (IsFdFamANDAssem(attrFlags)) AddString(L"protected internal ");
	if (IsFdFamORAssem(attrFlags)) AddString(L"protected OR internal ");

	if (IsFdStatic(attrFlags)) AddString(L"static ");
	if (IsFdStatic(attrFlags)) AddString(L"readonly ");
	if (IsFdLiteral(attrFlags)) AddString(L"const ");
	//IsFdNotSerialized

	if (IsFdPinvokeImpl(attrFlags)) AddString(L"pinvokeimpl ");
	if (IsFdSpecialName(attrFlags)) AddString(L"specialname ");
	if (IsFdHasFieldRVA(attrFlags)) AddString(L"RVA ");

	if (IsFdRTSpecialName(attrFlags)) AddString(L"rtspecialname ");
	if (IsFdHasFieldMarshal(attrFlags)) AddString(L"marshal ");
	if (IsFdHasDefault(attrFlags)) AddString(L"default ");

	//2 - next the type
	AddType(sigIt);

	AddString(L" ");
	if (className)
	{
		AddString(className);
		AddString(L"::");
	}
	AddString(name);

	return S_OK;
}

ULONG SigParser::AddType(PCCOR_SIGNATURE sigBlob)
{
	ULONG cb = 0;

	CorElementType type = (CorElementType)sigBlob[cb++];

	// Format the output 
	switch (type)
	{
	case ELEMENT_TYPE_VOID:     AddString(L"void"); break;
	case ELEMENT_TYPE_BOOLEAN:  AddString(L"bool"); break;
	case ELEMENT_TYPE_I1:       AddString(L"signed byte"); break;
	case ELEMENT_TYPE_U1:       AddString(L"byte"); break;
	case ELEMENT_TYPE_I2:       AddString(L"short"); break;
	case ELEMENT_TYPE_U2:       AddString(L"ushort"); break;
	case ELEMENT_TYPE_CHAR:     AddString(L"char"); break;
	case ELEMENT_TYPE_I4:       AddString(L"int"); break;
	case ELEMENT_TYPE_U4:       AddString(L"uint"); break;
	case ELEMENT_TYPE_I8:       AddString(L"long"); break;
	case ELEMENT_TYPE_U8:       AddString(L"ulong"); break;
	case ELEMENT_TYPE_R4:       AddString(L"float"); break;
	case ELEMENT_TYPE_R8:       AddString(L"double"); break;
	case ELEMENT_TYPE_OBJECT:   AddString(L"object"); break;
	case ELEMENT_TYPE_STRING:   AddString(L"string"); break;
	case ELEMENT_TYPE_I:        AddString(L"IntPtr"); break;
	case ELEMENT_TYPE_U:        AddString(L"UIntPtr"); break;
	case ELEMENT_TYPE_GENERICINST:
	{
		cb += AddType(&sigBlob[cb]);
		DWORD n;
		cb += CorSigUncompressData(&sigBlob[cb], &n);
		AddString(L"<");
		for (DWORD i = 0; i < n; i++)
		{
			if (i > 0)	 AddString(L",");

			cb += AddType(&sigBlob[cb]);
		}
		AddString(L">");
		break;
	}
	case ELEMENT_TYPE_MVAR:
	{
		DWORD ix;
		cb += CorSigUncompressData(&sigBlob[cb], &ix);
		WCHAR smallbuf[20];
		wsprintf(smallbuf, L"!!%d", ix);
		AddString(smallbuf);
		break;
	}
	case ELEMENT_TYPE_VAR:
	{
		DWORD ix;
		cb += CorSigUncompressData(&sigBlob[cb], &ix);
		AddString(L"!");
		WCHAR smallbuf[20];
		wsprintf(smallbuf, L"!%d", ix);
		AddString(smallbuf);
		break;
	}
	case ELEMENT_TYPE_VALUETYPE:
	case ELEMENT_TYPE_CLASS:
	{
		mdToken tk;
		cb += CorSigUncompressToken(&sigBlob[cb], &tk);

		wchar_t typeName[3000];
		ULONG typeNameLen;
		DWORD typeDefFlags;
		mdToken tkBaseClass;
		if (metaData->GetTypeDefProps(tk, typeName, sizeof(typeName), &typeNameLen, &typeDefFlags, &tkBaseClass) == S_OK)
		{
			AddString(typeName);
		}
		else
		{
			if ((tk & mdtTypeRef) > 0)
			{
				mdToken scopeTk;
				if (metaData->GetTypeRefProps(tk, &scopeTk, typeName, sizeof(typeName), &typeNameLen) == S_OK)
				{
					AddString(typeName);
				}
				else
				{
					AddString(L"<external typeRef>");
				}
				break;
			}
			AddString(L"<unknown type>");
		}
		break;
	}
	case ELEMENT_TYPE_TYPEDBYREF:
	{
		AddString(L"refany ");
		break;
	}
	case ELEMENT_TYPE_BYREF:
	{
		cb += AddType(&sigBlob[cb]);
		AddString(L"ref ");
		break;
	}
	case ELEMENT_TYPE_SZARRAY:      // Single Dim, Zero 
	{
		cb += AddType(&sigBlob[cb]);
		AddString(L"[]");
		break;
	}
	case ELEMENT_TYPE_ARRAY:        // General Array 
	{
		cb += AddType(&sigBlob[cb]);

		AddString(L"[");

		// Skip over rank 
		ULONG rank;
		cb += CorSigUncompressData(&sigBlob[cb], &rank);

		if (rank > 0)
		{
			// how many sizes? 
			ULONG sizes;
			cb += CorSigUncompressData(&sigBlob[cb], &sizes);

			// read out all the sizes 
			unsigned int i;

			for (i = 0; i < sizes; i++)
			{
				ULONG dimSize;
				cb += CorSigUncompressData(&sigBlob[cb], &dimSize);

				WCHAR smallbuf[20];
				wsprintf(smallbuf, L"!%d", dimSize);
				AddString(smallbuf);

				if (i > 0) AddString(L",");
			}

			// how many lower bounds? 
			ULONG lowers;
			cb += CorSigUncompressData(&sigBlob[cb], &lowers);

			// read out all the lower bounds. 
			for (i = 0; i < lowers; i++)
			{
				int lowerBound;
				cb += CorSigUncompressSignedInt(&sigBlob[cb], &lowerBound);
			}
		}
		else
		{
			AddString(L"?");
		}

		AddString(L"]");
		break;
	}
	case ELEMENT_TYPE_PTR:
	{
		cb += AddType(&sigBlob[cb]);
		AddString(L"* ");
		break;
	}
	case ELEMENT_TYPE_CMOD_REQD:
		AddString(L"CMOD_REQD");
		cb += AddType(&sigBlob[cb]);
		break;
	case ELEMENT_TYPE_CMOD_OPT:
		AddString(L"CMOD_OPT");
		cb += AddType(&sigBlob[cb]);
		break;
	case ELEMENT_TYPE_MODIFIER:
		cb += AddType(&sigBlob[cb]);
		break;
	case ELEMENT_TYPE_PINNED:
		AddString(L"pinned");
		cb += AddType(&sigBlob[cb]);
		break;
	case ELEMENT_TYPE_SENTINEL:
		break;
	default:
		AddString(L"<UNKNOWN TYPE>");
	}

	sigBlob += cb;

	return cb;
}

void SigParser::AddString(const wchar_t *str)
{
	VERIFY(wcscat_s(completeName.get(), completeNameLen, str) == 0);
}