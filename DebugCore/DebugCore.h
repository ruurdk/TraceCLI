#pragma once

//user-defined callback (not implemented yet)
#define customHandler function<void(void)>

//templates for loading values from a memory buffer
template<typename T> T inline load(unsigned char* addr)
{
	ASSERT(addr);
	return *reinterpret_cast<T*>(addr);
}
template<typename T, typename U> T inline load(unique_ptr<U[]> &addr)
{
	ASSERT(addr.get());
	return *reinterpret_cast<T*>(addr.get());
}
template<typename T, typename U> T inline load(U* addr)
{
	ASSERT(addr);
	return *reinterpret_cast<T*>(addr);
}

//CIL/MSIL opcodes (for detecting exit points)
#define CEE_OPCODE unsigned char

#define CEE_NOP (CEE_OPCODE)0
#define CEE_JMP (CEE_OPCODE)39 //<methodDef/methodRef>
#define CEE_RET (CEE_OPCODE)42 
#define EXITOPCODE(x) ((((CEE_OPCODE)x) == CEE_JMP) || (((CEE_OPCODE)x) == CEE_RET))

#define CEE_NEWOBJ (CEE_OPCODE)0x73 //<methodDef/Ref> of a .ctor
#define CEE_NEWARR (CEE_OPCODE)0x8D //<typeDef/Ref/Spec> of elements, can be valuetype
// #define CEE_INITOBJ (CEE_OPCODE)0xFE 0x15 //<typeDef/Ref/Spec> valueType initialization: not a huge inpact, only zeroes mem, no ctor called

#define CEE_BOX (CEE_OPCODE)0x8C //<typeDef/Def/Spec> put on stack, does nothing expensive for non-valuetypes (as it just puts obj on the stack)
#define CEE_UNBOX (CEE_OPCODE)0x79 //<typeDef/Ref/Spec> of a valueType, stack after op = ptr to valueType
#define CEE_UNBOXANY (CEE_OPCODE)0xA5 //<typeDef/Ref/Spec> of a valueType, stack after op = valueType (UNBOX.ANY = UNBOX + LDOBJ). For ref types effect = CASTCLASS <type>
#define CEE_UNBOXOPCODE(x) ((x == CEE_UNBOX) || (x == CEE_UNBOXANY))

#define CEE_CALL (CEE_OPCODE)0x28 //<methodDef/Ref/Spec> (call any (static/instance/global) method. type of object is compiletime specified: fully extracted from metadata token)
#define CEE_CALLI (CEE_OPCODE)0x29 //<methodSIG> (call function pointer which is already on the stack (following the arguments), signature of pointer is specified in methodSig>
#define CEE_CALLVIRT (CEE_OPCODE)0x6F //<methodDef/Ref/Spec> (always instance method - exact object is late bound: computed by CLR at runtime)

#define CEE_LDIND_I1 (CEE_OPCODE)0x46
//todo define builtin types inbetween as macros
#define CEE_LDIND_REF (CEE_OPCODE)0x50
#define CEE_STIND_REF (CEE_OPCODE)0x51
//todo define builtin types inbetween as macros
#define CEE_STIND_R8 (CEE_OPCODE)0x57

#define CEE_THROW (CEE_OPCODE)0x7A
#define CEE_LDFLD (CEE_OPCODE)0x7B
#define CEE_LDFLDA (CEE_OPCODE)0x7C
#define CEE_STFLD (CEE_OPCODE)0x7D

#define CEE_LDLEN (CEE_OPCODE)0x8E
#define CEE_LDELEMA (CEE_OPCODE)0x8F
#define CEE_LDELEM_I1 (CEE_OPCODE)0x90 
//todo define builtin types inbetween as macros
#define CEE_LDELEM_REF (CEE_OPCODE)0x9A
#define CEE_STELEM_I (CEE_OPCODE)0x9B
//todo define builtin types inbetween as macros
#define CEE_STELEM_REF (CEE_OPCODE)0xA2
#define CEE_LDELEM (CEE_OPCODE)0xA3
#define CEE_STELEM (CEE_OPCODE)0xA4


#define CEE_HASTYPETOKEN(x) ((x == CEE_JMP) || (x == CEE_NEWOBJ) || (x == CEE_NEWARR) || (x == CEE_BOX) || (x == CEE_UNBOX) || (x == CEE_UNBOXANY) || (x == CEE_CALL) || (x == CEE_CALLVIRT) || (x == CEE_LDELEM) || (x == CEE_STELEM) || (x == CEE_LDELEMA) || (x == CEE_LDFLD) || (x == CEE_LDFLDA) || (x == CEE_STFLD))
#define CEE_CANNULLREF(x) ((x == CEE_CALLVIRT) || (x == CEE_UNBOXANY) || (x >= CEE_THROW && x <= CEE_STFLD) || (x >= CEE_LDLEN && x <= CEE_STELEM) || (x >= CEE_LDIND_I1 && x <= CEE_STIND_R8))
#define CEE_OPBYTES(x) 1 //implement this when we define multibyte

const vector<wchar_t*> LdIndTypes = { L"System.SByte", L"System.Byte", L"System.Int16", L"System.UInt16", L"System.Int32", L"System.UInt32", L"System.Int64", L"System.IntPtr", L"System.Single", L"System.Double", L"System.Object" };
const vector<wchar_t*> StIndTypes = { L"System.Object", L"System.SByte", L"System.Int16", L"System.Int32", L"System.Int64", L"System.IntPtr", L"System.Single", L"System.Double" };
const vector<wchar_t*> StElemTypes = { L"System.IntPtr", L"System.SByte", L"System.Int16", L"System.Int32", L"System.Int64", L"System.Single", L"System.Double", L"System.Object" };

class OpcodeResolver
{	
public:	
	static const wchar_t* BuiltInTypeStringByOpcode(CEE_OPCODE opcode)
	{
		if (opcode >= CEE_LDIND_I1 && opcode <= CEE_LDIND_REF)
			return LdIndTypes[opcode - CEE_LDIND_I1];

		if (opcode >= CEE_STIND_REF && opcode <= CEE_STIND_R8)
			return StIndTypes[opcode - CEE_STIND_REF];

		if (opcode >= CEE_LDELEM_I1 && opcode <= CEE_LDELEM_REF)
			return LdIndTypes[opcode - CEE_LDELEM_I1];

		if (opcode >= CEE_STELEM_I && opcode <= CEE_STELEM_REF)
			return StElemTypes[opcode - CEE_STELEM_I];

		return nullptr;
	}
};

//simple structures
struct ProcInfo
{
	unsigned int pId;
	wchar_t* displayName;
	unsigned long numRuntimes;

	~ProcInfo()
	{
		if (displayName) delete[] displayName;
	}
};

struct RuntimeInfo
{
	wchar_t* runtimeFolder;
	wchar_t* versionString;

	~RuntimeInfo()
	{
		if (runtimeFolder) delete[] runtimeFolder;
		if (versionString) delete[] versionString;
	}
};

struct FieldInfo
{
	mdFieldDef  fieldToken;
	wchar_t		*fieldName;
	DWORD		fieldAttr;
	COR_SIGNATURE fieldSig;
	ULONG		fieldSigSize;
	DWORD		CPlusTypeFlags; //this is really a CorElementType
	wchar_t		*fieldConstValue;
	wchar_t		*parsedSignature;

	FieldInfo(mdFieldDef token, LPCWSTR name, DWORD attr, COR_SIGNATURE sig, ULONG sigSize, DWORD CPlusTypeFlags, wchar_t* fieldConstValue, LPCWSTR parsedSignature)
	{
		this->fieldToken = token;
		this->fieldAttr = attr;
		this->fieldSig = sig;
		this->fieldSigSize = sigSize;
		this->CPlusTypeFlags = CPlusTypeFlags;
		this->fieldConstValue = fieldConstValue;

		auto strlen = name ? wcslen(name) : 0;
		if (strlen == 0)
		{
			this->fieldName = L"<noname>";
		}
		else
		{
			this->fieldName = new wchar_t[strlen + 1];
			VERIFY(wcscpy_s(this->fieldName, strlen + 1, name) == 0);
		}

		if (parsedSignature)
		{
			this->parsedSignature = new wchar_t[wcslen(parsedSignature) + 1];
			this->parsedSignature[0] = '\0';
			VERIFY(wcscpy_s(this->parsedSignature, wcslen(parsedSignature) + 1, parsedSignature) == 0);
		}
	}

	~FieldInfo()
	{
		if (parsedSignature) delete[] this->parsedSignature;
		if (fieldConstValue) delete[] this->fieldConstValue;
		delete[] this->fieldName;
	}

	bool IsStatic() const
	{
		return !IsConst() && IsFdStatic(this->fieldAttr) != 0;
	}

	bool IsInstance() const
	{
		return !IsConst() && !IsStatic();
	}

	bool IsValueType() const
	{
		return CPlusTypeFlags != 0;
	}

	bool IsConst() const
	{
		return fieldConstValue != nullptr;
	}
};

struct MethodInfo
{
	MethodInfo(ULONG32 appDomainId, mdModule moduleToken, mdTypeDef classToken, mdMethodDef methodToken, ICorDebugFunction* corFunction,
	LPCWSTR appDomainName, LPCWSTR assemblyName, LPCWSTR className, LPCWSTR methodName,
	DWORD classFlags, DWORD methodAttrFlags, DWORD methodImplFlags, COR_SIGNATURE methodSigBytes, ULONG methodSigSize, LPCWSTR parsedSignature)
	{
		this->appDomainId = appDomainId;
		this->moduleToken = moduleToken;
		this->classToken = classToken;
		this->methodToken = methodToken;
		this->corFunction = corFunction;

		this->appDomainName = appDomainName;
		this->assemblyName = assemblyName;
		this->className = className;
		this->methodName = methodName;

		this->classFlags = classFlags;

		this->methodAttrFlags = methodAttrFlags;
		this->methodImplFlags = methodImplFlags;
		this->methodSig = methodSigBytes;
		this->methodSigSize = methodSigSize;

		if (parsedSignature)
		{
			this->parsedSignature = unique_ptr<wchar_t[]>(new wchar_t[wcslen(parsedSignature) + 1]);
			this->parsedSignature[0] = '\0';
			VERIFY(wcscpy_s(this->parsedSignature.get(), wcslen(parsedSignature) + 1, parsedSignature) == 0);
		}

		this->methodEntered = 0;
		this->avgTimeInMethod = 0.0;
		this->methodExitThroughException = 0;
	}

	ULONG32		appDomainId;
	LPCWSTR		appDomainName;

	LPCWSTR		assemblyName;
	mdModule	moduleToken;

	mdTypeDef	classToken;
	DWORD		classFlags;
	LPCWSTR		className;

	mdMethodDef		methodToken;
	DWORD			methodAttrFlags;
	DWORD			methodImplFlags;
	COR_SIGNATURE methodSig;
	ULONG			methodSigSize;
	ComPtr<ICorDebugFunction> corFunction;
	LPCWSTR			methodName;

	unique_ptr<wchar_t[]> parsedSignature;

	vector<shared_ptr<FieldInfo>> fieldsToReadOnBP;

	bool InstanceFieldsInToReadOnBP() const
	{
		for (auto fieldIt = fieldsToReadOnBP.begin(); fieldIt != fieldsToReadOnBP.end(); ++fieldIt)
		{
			if ((*fieldIt).get()->IsInstance()) return true;
		}
		return false;
	}

	CorPinvokeMap			nativeCallConv;
	CorCallingConvention	callConv;

	bool IsStatic() const
	{
		return IsMdStatic(methodAttrFlags) != 0;
	}

	volatile double avgTimeInMethod;
	volatile unsigned long methodEntered;
	volatile unsigned long methodExitThroughException;

	MethodInfo(ULONG32 appDomainId, mdModule moduleToken, mdTypeDef classToken, mdMethodDef methodToken, ICorDebugFunction* corFunction,
		DWORD classFlags, DWORD methodAttrFlags, DWORD methodImplFlags, COR_SIGNATURE methodSigBytes, ULONG methodSigSize, LPCWSTR parsedSignature)
		: MethodInfo(appDomainId, moduleToken, classToken, methodToken, corFunction, nullptr, nullptr, nullptr, nullptr,
		classFlags, methodAttrFlags, methodImplFlags, methodSigBytes, methodSigSize, parsedSignature) {}
};

struct BreakpointInfo
{
	shared_ptr<MethodInfo> method;
	ULONG ilOffset;

	customHandler userHandler;

	CEE_OPCODE CILInstruction; //only relevant if ilOffset != 0
	mdToken typeToken;		   //some instructions are followed by a typedef/ref/spec, or methodDef/Ref. Context determines what this is

	volatile ULONG hitCount;

	//std vector sorting
	bool operator < (const BreakpointInfo& str) const
	{
		return (hitCount < str.hitCount);
	}

	bool IsEntryBreakpoint() const
	{
		return (CILInstruction == CEE_NOP && ilOffset == 0);
	}

	bool IsExitBreakpoint() const
	{
		return EXITOPCODE(CILInstruction);
	}
};

struct ByHitCountPtr : public std::binary_function<shared_ptr<BreakpointInfo>, shared_ptr<BreakpointInfo>, bool>
{
	inline bool operator() (const shared_ptr<BreakpointInfo>& bp1, const shared_ptr<BreakpointInfo>& bp2) const
	{
		return (bp1.get()->hitCount < bp2.get()->hitCount);
	}
};

struct ByTimeSpentPtr : public std::binary_function<shared_ptr<MethodInfo>, shared_ptr<MethodInfo>, bool>
{
	inline bool operator() (const shared_ptr<MethodInfo>& m1, const shared_ptr<MethodInfo>& m2) const
	{
		return m1.get()->avgTimeInMethod * (double)m1.get()->methodEntered < m2.get()->avgTimeInMethod * (double)m2.get()->methodEntered;
	}
};

//map sorting
struct ByHitcount : public std::binary_function<BreakpointInfo, BreakpointInfo, bool>
{
	bool operator()(const BreakpointInfo& lhs, const BreakpointInfo& rhs) const
	{
		return lhs.hitCount < rhs.hitCount;
	}
};