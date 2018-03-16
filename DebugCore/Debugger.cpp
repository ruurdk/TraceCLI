#include "precompiled.h"

#include "Debugger.h"
#include "LegacyManagedDebugger.h"

Debugger::Debugger(OPMODE mode) : pId(0)
{
	this->mode = mode;

	LARGE_INTEGER clockFreq;
	VERIFY(QueryPerformanceFrequency(&clockFreq));
	timerFreq = (double)clockFreq.QuadPart;
}

Debugger::Debugger(OPMODE mode, DWORD pId) : pId(0)
{
	this->mode = mode;

	LARGE_INTEGER clockFreq;
	VERIFY(QueryPerformanceFrequency(&clockFreq));
	timerFreq = (double)clockFreq.QuadPart;

	if (!DoAttach(pId)) throw new exception("Failed to attach to live process");
}

//todo: attach to dump

bool Debugger::DoAttach(DWORD pId)
{
	TRACE(L"Attach debugger to process\n");
	if (!IsAttached())
	{
		//managed attach
		try
		{
			this->DebugClientManaged = unique_ptr<IDebuggerImplementation>(new LegacyManagedDebugger(static_cast<IDebugger*>(this), pId)); //live target (v2 API)
			//pDebugClientManaged = new DbgEngDataTarget(pDebugClient, pId);    //inspection only live target (v4 API)

			// metadata resolver
			auto pProcess = DebugClientManaged->CorProcess();
			ASSERT(pProcess);

			this->MetaInfo = unique_ptr<MetaHelpers>(new MetaHelpers(pProcess));
		}
		catch (exception& ex)
		{
			TRACE(L"Error starting managed debugging: %S\n", ex.what());
			return false;
		}

		//(native) attach 		
		if ((DebugCreate(__uuidof(IDebugClient), &DebugClient) == S_OK)
			&& (DebugClient->AttachProcess(0, pId, DEBUG_ATTACH_NONINVASIVE | DEBUG_ATTACH_NONINVASIVE_NO_SUSPEND) == S_OK))
		{
			TRACE(L"Native attach success\n");

			//make sure we only continue after native attach
			ComPtr<IDebugControl> DebugControl;
			if (DebugClient.As(&DebugControl) == S_OK)
			{
				DebugControl->SetExecutionStatus(DEBUG_STATUS_GO); //this will wait for attach before returning
				if ((DebugControl->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE)) != S_OK)
				{
					return false;
				}
				//todo implement native event & output callbacks (might prevent crashes)
				//pDebugClient->SetOutputCallbacks()					
				TRACE(L"Resumed native target execution\n");
			}
		}
		else
		{
			TRACE(L"Native attach failed!\n");
		}

		TRACE(L"Debugger operational\n");

		return IsAttached();
	}
	return false;
}

void Debugger::Detach()
{
	TRACE(L"Detach debugger\n");
	if (IsAttached())
	{
		if (DebugClient)
		{
			DebugClient->DetachProcesses();
			DebugClient = nullptr;
		}

		DebugClientManaged = nullptr;
		MetaInfo = nullptr;
	}
}

bool Debugger::IsAttached() const
{
	return (DebugClientManaged != nullptr);
}

void Debugger::Continue()
{
	ASSERT(DebugClientManaged);

	DebugClientManaged->CorProcess()->Continue(false);
}

void Debugger::Stop()
{
	ASSERT(DebugClientManaged);

	DebugClientManaged->CorProcess()->Stop(5000);
}

Debugger::~Debugger()
{
	BOOL isActive;

	TRACE(L"Clearing breakpoints\n");

	for (auto bpIt = managedBPs.begin(); bpIt != managedBPs.end(); ++bpIt)
	{
		auto corBP = (*bpIt).first;
		if (corBP->IsActive(&isActive) == S_OK && isActive) corBP->Activate(false);
		corBP->Release();
	}
	managedBPs.clear();

	Detach();
}

HRESULT Debugger::GetAppDomains(ICorDebugProcess *pProcess, ICorDebugAppDomain **ppDomains, ULONG maxDomains, ULONG *loadedDomains)
{
	ASSERT(pProcess);

	ComPtr<ICorDebugAppDomainEnum> appDomainEnum;
	if (pProcess->EnumerateAppDomains(&appDomainEnum) == S_OK)
	{
		appDomainEnum->Next(maxDomains, ppDomains, loadedDomains);
		if (!*loadedDomains)
		{
			TRACE(L"Failed to retreive process app domain enumerator\n");
			return E_FAIL;
		}
		return S_OK;
	}
	TRACE(L"Failed to enumerate process app domains\n");
	return E_FAIL;
}

HRESULT Debugger::GetRuntimeAssemblies(ICorDebugAppDomain *pAppDomain, ICorDebugAssembly **ppAssemblies, ULONG maxAssemblies, ULONG *loadedAssemblies)
{
	ASSERT(pAppDomain);

	ULONG32 appDomainId = 0;
	ComPtr<ICorDebugAssemblyEnum> assemblyEnum;
	if (pAppDomain->EnumerateAssemblies(&assemblyEnum) == S_OK)
	{
		assemblyEnum->Next(maxAssemblies, ppAssemblies, loadedAssemblies);
		if (!*loadedAssemblies)
		{
			pAppDomain->GetID(&appDomainId);
			TRACE(L"Failed to retreive assembly enumerator for appdomain %u\n", appDomainId);
			return E_FAIL;
		}
		return S_OK;
	}
	pAppDomain->GetID(&appDomainId);
	TRACE(L"Failed to enumerate assemblies for domain %u\n", appDomainId);

	return E_FAIL;
}

HRESULT Debugger::GetRuntimeModules(ICorDebugAssembly *pAssembly, ICorDebugModule **ppModules, ULONG maxModules, ULONG *loadedModules)
{
	ASSERT(pAssembly);

	ComPtr<ICorDebugModuleEnum> moduleEnum;
	if (pAssembly->EnumerateModules(&moduleEnum) == S_OK)
	{
		moduleEnum->Next(maxModules, ppModules, loadedModules);
		if (!*loadedModules)
		{
			TRACE(L"Failed to retreive module enumerator for assembly\n");
			return E_FAIL;
		}
		return S_OK;
	}
	TRACE(L"Failed to enumerate modules for assembly\n");
	return E_FAIL;
}

void Debugger::FindManagedMethods(wchar_t * namespaceFilterName, const wchar_t * classFilterName, const wchar_t * methodFilterName, const vector<const wchar_t *> &fields, vector<shared_ptr<MethodInfo>> &functions)
{
	TRACE(L"Find methods in target. Namespace filter %s, class filter %s, method filter %s\n", namespaceFilterName, classFilterName, methodFilterName);
	//wprintf_s(L"Find methods in target. Namespace filter %s, class filter %s, method filter %s\n", namespaceFilterName, classFilterName, methodFilterName);

	if (DebugClientManaged)
	{
		auto pProcess = DebugClientManaged->CorProcess();
		ASSERT(pProcess);

		//foreach appDomain
		ICorDebugAppDomain *appDomains[20];
		ULONG numDomains = 0;

		if (GetAppDomains(pProcess, appDomains, sizeof(appDomains), &numDomains) == S_OK)
		{
			for (ULONG domainIt = 0; domainIt < numDomains; domainIt++)
			{
				wchar_t domainName[1024];
				ULONG32 domainNameLen;
				VERIFY(appDomains[domainIt]->GetName(sizeof(domainName), &domainNameLen, domainName) == S_OK);
				ULONG32 domainId;
				VERIFY(appDomains[domainIt]->GetID(&domainId) == S_OK);
				TRACE(L"Searching domain: %s\n", domainName);
				LOG(L"Searching domain: %s\n", domainName);

				//foreach assembly
				ICorDebugAssembly *assemblies[500];
				ULONG numAssemblies = 0;
				if (GetRuntimeAssemblies(appDomains[domainIt], assemblies, sizeof(assemblies), &numAssemblies) == S_OK)
				{
					for (ULONG asmIt = 0; asmIt < numAssemblies; asmIt++)
					{
						wchar_t assemblyName[1024];
						ULONG32 assemblyNameLen;
						VERIFY(assemblies[asmIt]->GetName(sizeof(assemblyName), &assemblyNameLen, assemblyName) == S_OK);
						TRACE(L"Searching assembly: %s\n", assemblyName);
						LOG(L"Searching assembly: %s\n", assemblyName);
						wprintf_s(L".");

						ICorDebugModule *modules[30];
						ULONG numModules = 0;
						//foreach module
						if (GetRuntimeModules(assemblies[asmIt], modules, sizeof(modules), &numModules) == S_OK)
						{
							mdModule moduleToken;
							for (ULONG modIt = 0; modIt < numModules; modIt++)
							{
								VERIFY(modules[modIt]->GetToken(&moduleToken) == S_OK);

								//get types
								ComPtr<IMetaDataImport2> modMeta;
								if (modules[modIt]->GetMetaDataInterface(IID_IMetaDataImport, &modMeta) == S_OK)
								{
									BOOL dynamic = false;
									modules[modIt]->IsDynamic(&dynamic);

									//foreach type
									mdTypeDef moduleTypes[20480];
									ULONG numTypes;
									HCORENUM corenum = nullptr;
									if (modMeta->EnumTypeDefs(&corenum, moduleTypes, sizeof(moduleTypes), &numTypes) == S_OK)
									{
										HCORENUM typecorenum;

										// add mdTypeDefNil
										moduleTypes[numTypes] = mdTypeDefNil;
										numTypes++;

										for (ULONG typeIt = 0; typeIt < numTypes; typeIt++)
										{
											//get type info
											wchar_t className[20480];
											ULONG classNameLen;
											DWORD typeDefFlags;
											mdToken mdBaseType;
											if ((modMeta->GetTypeDefProps(moduleTypes[typeIt], className, sizeof(className), &classNameLen, &typeDefFlags, &mdBaseType) != S_OK)) continue;

											//cache the type name
											auto nameCopy = new wchar_t[classNameLen + 1];
											VERIFY(wcscpy_s(nameCopy, classNameLen + 1, className) == 0);
											MetaInfo->AddToCache(moduleTypes[typeIt], shared_ptr<wchar_t>(nameCopy));

											if (dynamic)
											{
												LOG(L"Type %s found\n", className);
											}

											//is the type a class ?
											//if ((!IsTdClass(typeDefFlags)) && (!IsTdInterface(typeDefFlags)))
											//		continue; 

											auto typeIsFiltered = false;

											//compare name against classname filter
											if (classFilterName != nullptr)
											{
												if (wcsncmp(className, classFilterName, wcslen(classFilterName)) != 0) typeIsFiltered = true;
											}

											//compare name against namespace filter
											if (namespaceFilterName != nullptr)
											{
												if (wcsncmp(className, namespaceFilterName, wcslen(namespaceFilterName)) != 0) typeIsFiltered = true;
											}

											//todo: check against baseclass (can this be assigned to baseclass ?)


											//get methods
											typecorenum = nullptr;
											mdMethodDef methods[12800];
											ULONG numMethods;
											if ((modMeta->EnumMethods(&typecorenum, moduleTypes[typeIt], methods, sizeof(methods), &numMethods) == S_OK) && (numMethods > 0))
											{
												mdTypeDef mdClass;
												wchar_t methodName[20480];
												ULONG methodNameLen;
												DWORD methodAttrFlags;
												PCCOR_SIGNATURE methodSig;
												ULONG methodSigSize;
												ULONG methodRVA;
												DWORD methodImplFlags;
												for (ULONG methodIt = 0; methodIt < numMethods; methodIt++)
												{
													//get method info
													if (modMeta->GetMethodProps(methods[methodIt], &mdClass, methodName, sizeof(methodName), &methodNameLen, &methodAttrFlags, &methodSig, &methodSigSize, &methodRVA, &methodImplFlags) == S_OK)
													{
														//parse signature
														auto mSigP = unique_ptr<SigParser>(new SigParser(className, methodSig, methodSigSize, methodName, modMeta.Get(), methods[methodIt], methodImplFlags, methodAttrFlags));

														if (moduleTypes[typeIt] == mdTypeDefNil)
														{
															TRACE(L"Global method: %s\n", methodName);
														}

														//cache name
														auto namebufsize = wcslen(mSigP->Signature()) + 1;
														auto methodNameCopy = new wchar_t[namebufsize];
														VERIFY(wcscpy_s(methodNameCopy, namebufsize, mSigP->Signature()) == 0);
														MetaInfo->AddToCache(methods[methodIt], shared_ptr<wchar_t>(methodNameCopy));
														//TRACE(L"Add to membercache: %s %X\n", methodNameCopy, methods[methodIt]);

														//only came this far to catch the method signature, early out now
														if (typeIsFiltered) continue;

														//compare name against methodname filter
														if (methodFilterName != nullptr)
														{
															if (wcsncmp(methodName, methodFilterName, wcslen(methodFilterName)) != 0) continue;
														}

														//methods with no implementation in managed code can't be breaked into
														if (IsMiForwardRef(methodImplFlags))
														{
															LOG(L"Can not instrument method %s because it's a Forward Reference\n", methodName);
															continue;
														}

														if (IsMiInternalCall(methodImplFlags))
														{
															LOG(L"Can not instrument method %s because it's an internal call (ECALL)\n", methodName);
															continue;
														}

														if (IsMiUnmanaged(methodImplFlags))
														{
															LOG(L"Can not instrument method %s because it's unmanaged\n", methodName);
															continue;
														}

														//abstract methods have no bodies
														if (IsMdAbstract(methodAttrFlags))
														{
															LOG(L"Can not instrument method %s because it's abstract (no body)\n", methodName);
															continue;
														}

														//get the corresponding debug function
														ICorDebugFunction *pFunction;
														if (modules[modIt]->GetFunctionFromToken(methods[methodIt], &pFunction) == S_OK)
														{
															//OutputDebugString(L"Found method:"); OutputDebugString(className); OutputDebugString(L"::"); OutputDebugString(methodName); OutputDebugString(L"\n");													
															TRACE(L"Found method: %s\n", mSigP->Signature());

															auto newMethodInfo = shared_ptr<MethodInfo>(new MethodInfo(domainId, moduleToken, mdClass, methods[methodIt], pFunction,
																typeDefFlags, methodAttrFlags, methodImplFlags, *methodSig, methodSigSize, mSigP->Signature()));

															//resolve calling convention
															ULONG nativeCallingConv;
															if (modMeta->GetNativeCallConvFromSig(methodSig, methodSigSize, &nativeCallingConv) == S_OK)
															{
																newMethodInfo->nativeCallConv = (CorPinvokeMap)nativeCallingConv;
															}
															ULONG manCallConv;
															if (CorSigUncompressCallingConv(methodSig, methodSigSize, &manCallConv) == S_OK)
															{
																newMethodInfo->callConv = (CorCallingConvention)manCallConv;
															}

															functions.push_back(newMethodInfo);

															//find fields to load on BP hit
															for (auto searchFieldIt = fields.begin(); searchFieldIt != fields.end(); ++searchFieldIt)
															{
																HCORENUM fieldCoreEnum = nullptr;
																mdFieldDef fieldArray[500];
																ULONG foundFields;
																if (modMeta->EnumFieldsWithName(&fieldCoreEnum, moduleTypes[typeIt], *searchFieldIt, fieldArray, sizeof(fieldArray), &foundFields) == S_OK)
																{
																	//get field info
																	mdTypeDef mdClassWeAlreadyKnow;
																	wchar_t fieldName[2048];
																	ULONG fieldNameLen;
																	DWORD fieldAttr;
																	PCCOR_SIGNATURE fieldSig;
																	ULONG fieldSigSize;
																	DWORD CPlusTypeFlags; //value type of field
																	UVCP_CONSTANT fieldValue;
																	ULONG fieldValueSize;
																	for (ULONG fieldNum = 0; fieldNum < foundFields; fieldNum++)
																	{
																		if (modMeta->GetFieldProps(fieldArray[fieldNum], &mdClassWeAlreadyKnow, fieldName, sizeof(fieldName), &fieldNameLen, &fieldAttr, &fieldSig, &fieldSigSize, &CPlusTypeFlags, &fieldValue, &fieldValueSize) == S_OK)
																		{
																			if (IsMdStatic(methodAttrFlags) && !IsFdStatic(fieldAttr))
																			{
																				TRACE(L"Found an instance field: %s but it can't be reached by static method %s::%s\n", fieldName, className, methodName);
																				continue;
																			}

																			//if the field is a constant parse its value as a string
																			wchar_t * constantFieldString = nullptr;
																			if (CPlusTypeFlags && fieldValue)
																			{
																				VERIFY(GetConstValue(CPlusTypeFlags, fieldValue, fieldValueSize, &constantFieldString) == S_OK);
																			}

																			auto sigP = unique_ptr<SigParser>(new SigParser(className, fieldSig, fieldSigSize, fieldName, modMeta.Get(), fieldArray[fieldNum], 0, fieldAttr));

																			TRACE(L"Found field: %s\n", sigP->Signature());

																			auto fieldInfo = shared_ptr<FieldInfo>(new FieldInfo(fieldArray[fieldNum], fieldName, fieldAttr, *fieldSig, fieldSigSize, CPlusTypeFlags, constantFieldString, sigP->Signature()));
																			newMethodInfo->fieldsToReadOnBP.push_back(fieldInfo);
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
								else
								{
									LOG(L"Failed to get metadata for module\n");
								}
								modules[modIt]->Release();
							}
						}
						assemblies[asmIt]->Release();
					}
				}
				appDomains[domainIt]->Release();
			}
		}
	}
}

HRESULT Debugger::SetBPAtEntry(shared_ptr<MethodInfo> pFunction, customHandler handler)
{
	//direct (high level) function breakpoint
	ICorDebugFunctionBreakpoint *newBP;
	if (pFunction->corFunction->CreateBreakpoint(&newBP) == S_OK)
	{
		auto bpInfo = shared_ptr<BreakpointInfo>(new BreakpointInfo{});
		bpInfo->method = pFunction;
		bpInfo->ilOffset = 0;
		bpInfo->userHandler = handler;
		bpInfo->CILInstruction = CEE_NOP;

		//global cache of BPs
		managedBPs[newBP] = bpInfo;

		return S_OK;
	}

	//IL BP
	ComPtr<ICorDebugCode> code;
	if (pFunction->corFunction->GetILCode(&code) == S_OK)
	{
		if (code->CreateBreakpoint(0, &newBP) == S_OK)
		{
			auto bpInfo = shared_ptr<BreakpointInfo>(new BreakpointInfo{});
			bpInfo->method = pFunction;
			bpInfo->ilOffset = 0;
			bpInfo->userHandler = handler;
			bpInfo->CILInstruction = CEE_NOP;

			//global cache of BPs
			managedBPs[newBP] = bpInfo;

			return S_OK;
		}
	}

	//native breakpoint
	if (pFunction->corFunction->GetNativeCode(&code) == S_OK)
	{
		if (code->CreateBreakpoint(0, &newBP) == S_OK)
		{
			auto bpInfo = shared_ptr<BreakpointInfo>(new BreakpointInfo{});
			bpInfo->method = pFunction;
			bpInfo->ilOffset = 0;
			bpInfo->userHandler = handler;
			bpInfo->CILInstruction = CEE_NOP;

			//global cache of BPs
			managedBPs[newBP] = bpInfo;

			return S_OK;
		}
	}

	//ultimate fallback, leverage native debugger

	TRACE(L"Failed to set breakpoint at function entry\n");
	return E_FAIL;
}

int Debugger::SetBPAtExit(shared_ptr<MethodInfo> pFunction, customHandler handler)
{
	ComPtr<ICorDebugCode> ilCode;
	BOOL isIL;
	int retval = 0;

	if ((pFunction->corFunction->GetILCode(&ilCode) == S_OK) && (ilCode != nullptr) && (ilCode->IsIL(&isIL) == S_OK) && isIL)
	{
		//todo: use the non-deprecated GetCode
		ULONG32 codeSize;
		if (ilCode->GetSize(&codeSize) == S_OK)
		{
			auto buffer = new byte[codeSize];
			ULONG32 returnedCode;
			if (GetCode(ilCode.Get(), codeSize, buffer, &returnedCode) == S_OK)
			{
				int lastExitBP = -1;
				for (ULONG32 codeIt = 0; codeIt < returnedCode; codeIt++)
				{
					if (EXITOPCODE(buffer[codeIt]))
					{
						ICorDebugFunctionBreakpoint *bp;
						if (ilCode->CreateBreakpoint(codeIt, &bp) == S_OK)
						{
							auto bpInfo = shared_ptr<BreakpointInfo>(new BreakpointInfo{});
							bpInfo->method = pFunction;
							bpInfo->ilOffset = codeIt;
							bpInfo->userHandler = handler;
							bpInfo->CILInstruction = buffer[codeIt];

							managedBPs[bp] = bpInfo;

							lastExitBP = codeIt;
							retval++;
						}
						else
						{
							LOG(L"Failed to set breakpoint at location %u, trying to find new bind location in the range %u<%u\n", codeIt, lastExitBP + 1, codeIt - 1);
							//todo: set breakpoint at highest possible IL offset before this one...or get native sequence points and select the right one
							for (int backIt = codeIt - 1; backIt > lastExitBP; backIt--)
							{
								if (ilCode->CreateBreakpoint(backIt, &bp) == S_OK)
								{
									auto bpInfo = shared_ptr<BreakpointInfo>(new BreakpointInfo{});
									bpInfo->method = pFunction;
									bpInfo->ilOffset = backIt;
									bpInfo->userHandler = handler;
									bpInfo->CILInstruction = buffer[codeIt];

									managedBPs[bp] = bpInfo;

									lastExitBP = codeIt;
									retval++;

									LOG(L"Found available exit breakpoint location for IL location %u at location %u\n", codeIt, backIt);

									break;
								}
							}
						}
					}
				}
			}
			delete[] buffer;
		}
	}
	return retval;
}

HRESULT Debugger::GetCode(ICorDebugCode *code, ULONG32 bufferSize, byte* buffer, ULONG32 *numBytes)
{
	ULONG32 maxChunks = 300;
	ULONG32 numChunks = 0;
	CodeChunkInfo chunkInfo[300];

	ULONG32 bufferIndex = 0;
	//try to do it with non deprecated ICorDebugCode2
	ComPtr<ICorDebugCode2> code2;
	if (code->QueryInterface(__uuidof(ICorDebugCode2), &code2) == S_OK)
	{
		if (code2->GetCodeChunks(maxChunks, &numChunks, chunkInfo) == S_OK)
		{
			ASSERT(DebugClientManaged);
			auto pProcess = DebugClientManaged->CorProcess();
			ASSERT(pProcess);

			for (ULONG32 chunkId = 0; chunkId < numChunks; chunkId++)
			{
				auto thisChunk = chunkInfo[chunkId];

				ULONG32 bytesRead = 0;
				VERIFY(ReadMemory(pProcess, thisChunk.startAddr, buffer + bufferIndex, thisChunk.length, &bytesRead) == S_OK);

				*numBytes += bytesRead;
				bufferIndex += bytesRead;
			}
			return S_OK;
		}
	}
	else
	{ //fall back to deprecated v1 getcode
		ULONG32 codeSize = 0;
		if (code->GetSize(&codeSize) == S_OK)
		{
			return code->GetCode(0, codeSize, bufferSize, buffer, numBytes);
		}
	}
	return E_FAIL;
}

int Debugger::SetBPAtOpCode(shared_ptr<MethodInfo> pFunction, customHandler handler, CEE_OPCODE opcode)
{
	ComPtr<ICorDebugCode> ilCode;
	BOOL isIL;
	int retval = 0;

	if ((pFunction->corFunction->GetILCode(&ilCode) == S_OK) && (ilCode != nullptr) && (ilCode->IsIL(&isIL) == S_OK) && isIL)
	{
		//todo: use the non-deprecated GetCode
		ULONG32 codeSize;
		if (ilCode->GetSize(&codeSize) == S_OK)
		{
			auto buffer = new byte[codeSize];
			ULONG32 returnedCode;
			if (GetCode(ilCode.Get(), codeSize, buffer, &returnedCode) == S_OK)
			{
				//retval = S_OK; // not all methods have newobj, getting the IL is enough for success
				for (ULONG32 codeIt = 0; codeIt < returnedCode; codeIt++)
				{
					if (buffer[codeIt] == opcode)
					{
						ICorDebugFunctionBreakpoint *bp;
						//set bp at exact location or nearest greater IL IP
						auto codeItDelta = codeIt;
						HRESULT res;
						do
						{
							res = ilCode->CreateBreakpoint(codeItDelta, &bp);
							codeItDelta++;
						} while (res != S_OK && codeItDelta < codeSize);

						if (res == S_OK)
						{
							auto bpInfo = shared_ptr<BreakpointInfo>(new BreakpointInfo{});
							bpInfo->method = pFunction;
							bpInfo->ilOffset = codeIt;
							bpInfo->userHandler = handler;
							bpInfo->CILInstruction = opcode;

							//optional type token
							if (CEE_HASTYPETOKEN(opcode))
							{
								auto metaToken = load<mdToken>(&buffer[codeIt + CEE_OPBYTES(opcode)]);
								bpInfo->typeToken = metaToken;

								MetaInfo->ResolveTokenAndAddToCache(metaToken, pFunction->corFunction.Get());
							}

							managedBPs[bp] = bpInfo;

							retval++;
						}
					}
				}
			}
			delete[] buffer;
		}
	}
	return retval;
}


void Debugger::ActivateBPs(BOOL active)
{
	TRACE(L"%s all %u registered breakpoints\n", active ? L"Activate" : L"Deactivate", managedBPs.size());

	Stop();
	for (auto bpIt = managedBPs.begin(); bpIt != managedBPs.end(); ++bpIt)
	{
		auto bp = bpIt->first;
		BOOL state;
		if ((bp->IsActive(&state) == S_OK) && (state != active))
		{
			if (bp->Activate(active) != S_OK)
			{
				TRACE(L"Failed to %s breakpoints\n", active ? L"activate " : L"deactivate ");
			}
		}
	}
	Continue();

	TRACE(L"Breakpoints %s\n", active ? L"activated" : L"deactivated");

	return;
}

void Debugger::OnBreakpointHit(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugBreakpoint &Breakpoint)
{
	//time it before anything else
	LARGE_INTEGER bpTime;
	if (!QueryPerformanceCounter(&bpTime)) return;

	auto currentTime = bpTime.QuadPart;

	//is it a function breakpoint (only thing we support) ?
	ComPtr<ICorDebugFunctionBreakpoint> fBP;
	if (Breakpoint.QueryInterface(__uuidof(ICorDebugFunctionBreakpoint), &fBP) != S_OK) return;

	//is it in our global cache ?
	auto pos = managedBPs.find(fBP.Get());
	if (pos == managedBPs.end()) return;

	//find the details about this breakpoint
	auto mInfo = pos->second->method;

	//register hit
	InterlockedIncrement(&(pos->second->hitCount));

	//timing
	if (mode & OPMODE_TIMINGS)
	{
		auto entryBP = pos->second->IsEntryBreakpoint();
		auto exitBP = pos->second->IsExitBreakpoint();

		DWORD threadId;
		if ((exitBP || entryBP) && (Thread.GetID(&threadId) == S_OK))
		{
			auto crudeHash = pos->second->method->methodToken ^ threadId;

			if (exitBP)
			{
				//check for pending timer on this methodInfo + threadId get time spent
				auto timerSearch = pendingTimers.find(crudeHash);
				if (timerSearch != pendingTimers.end())
				{
					auto pendingInfo = (*timerSearch).second;
					//remove from pending
					pendingTimers.erase(timerSearch);

					auto timeDiff = ((double)currentTime - pendingInfo.timeStamp) / timerFreq;

					//update methodInfo
					auto totalTime = (mInfo->avgTimeInMethod * (double)(mInfo->methodEntered - 1)) + timeDiff;
					mInfo->avgTimeInMethod = totalTime / (double)mInfo->methodEntered;
				}
			}
			else
			{
				//create pending timer for this methodInfo + threadId
				pendingTimers[crudeHash] = PendingTimer{ currentTime, threadId, pos->second->method->methodToken, pos->second->method };

				//increment method entered count
				InterlockedIncrement(&(mInfo->methodEntered));
			}
		}
	}

	//no field dumping ? -> return
	if ((mode & OPMODE_FIELDS) == 0) return;

	//check if this is an entry breakpoint
	if (!pos->second->IsEntryBreakpoint()) return;

	auto process5 = DebugClientManaged->CorProcess5();
	ASSERT(process5);

	//do we need an object (for instance fields) ?
	if (!mInfo->IsStatic() && mInfo->InstanceFieldsInToReadOnBP())
	{
		//try to get 'this' pointer
		ComPtr<ICorDebugObjectValue> thisPtr = nullptr;

		//1 - try to get it from the managed stack
		ComPtr<ICorDebugFrame> frame2;
		if (Thread.GetActiveFrame(&frame2) == S_OK)
		{
			ComPtr<ICorDebugILFrame> ilFrame;
			if (frame2.As(&ilFrame) == S_OK)
			{
				ComPtr<ICorDebugValue> arg0;
				if (ilFrame->GetArgument(0, &arg0) == S_OK)
				{
					//now to get the actual instance
					ComPtr<ICorDebugReferenceValue> refThis;
					if (arg0.As(&refThis) == S_OK)
					{
						ComPtr<ICorDebugValue> actualThis;
						if (refThis->Dereference(&actualThis) == S_OK)
						{
							//double check it's an object
							actualThis.As(&thisPtr);
						}
					}
				}
			}
		}

		//2 - try to get it from the register stack (ECX should have it)
		if (!thisPtr)
		{
			//try to get a Process5...
			ComPtr<ICorDebugRegisterSet> registers;
			if (Thread.GetRegisterSet(&registers) == S_OK)
			{
				CONTEXT threadCtx = CONTEXT{};
				threadCtx.ContextFlags = CONTEXT_INTEGER;

				if (registers->GetThreadContext(sizeof(threadCtx), (BYTE*)&threadCtx) == S_OK)
				{
#ifdef _WIN64
					auto Ecx = threadCtx.Rcx;
#else
					auto Ecx = threadCtx.Ecx;
#endif
					process5->GetObjectW(Ecx, &thisPtr);
				}
			}
		}

		//do something with the object (read its fields) if we have it
		if (thisPtr)
		{
			LOG(L"Method entry: %s.%s::%s\n", mInfo->assemblyName, mInfo->className, mInfo->methodName);
			ComPtr<ICorDebugClass> thisClass;
			if (thisPtr->GetClass(&thisClass) == S_OK)
			{
				for (auto fieldIt = mInfo->fieldsToReadOnBP.begin(); fieldIt != mInfo->fieldsToReadOnBP.end(); fieldIt++)
				{
					auto fieldInfo = *fieldIt;
					if (fieldInfo->IsStatic()) continue;

					ComPtr<ICorDebugValue> fieldValue;
					if (thisPtr->GetFieldValue(thisClass.Get(), fieldInfo->fieldToken, &fieldValue) == S_OK)
					{
						VERIFY(DumpFieldValue(fieldInfo->parsedSignature, fieldValue) == S_OK);
					}
				}
			}
		}
		else
		{
			TRACE(L"Couldn't find 'this' pointer on stack, skipping instance field dump\n");
		}
	}

	//dump static fields we want
	ComPtr<ICorDebugClass> classForStatics;
	ComPtr<ICorDebugFrame> frame;
	if (mInfo->corFunction->GetClass(&classForStatics) == S_OK)
	{
		if (Thread.GetActiveFrame(&frame) == S_OK)
		{
			ComPtr<ICorDebugValue> staticField;

			auto fieldsToRead = mInfo->fieldsToReadOnBP;
			for (auto fieldIt = fieldsToRead.begin(); fieldIt != fieldsToRead.end(); ++fieldIt)
			{
				auto fieldInfo = *fieldIt;
				if (!fieldInfo->IsStatic()) continue;

				if (classForStatics->GetStaticFieldValue(fieldInfo->fieldToken, frame.Get(), &staticField) == S_OK)
				{
					VERIFY(DumpFieldValue(fieldInfo->parsedSignature, staticField) == S_OK);
				}
			}
		}
	}

	//dump const fields we want
	auto fieldsToRead = mInfo->fieldsToReadOnBP;
	for (auto fieldIt = fieldsToRead.begin(); fieldIt != fieldsToRead.end(); ++fieldIt)
	{
		auto fieldInfo = *fieldIt;
		if (!fieldInfo->IsConst()) continue;

		TRACE(L"Field: %s Value: %s (const)\n", fieldInfo->parsedSignature, fieldInfo->fieldConstValue);
		LOG(L"Field: %s, Value: %s (const)\n", fieldInfo->parsedSignature, fieldInfo->fieldConstValue);
	}
}

// Rich logging of some exceptions.		
void Debugger::LogExceptionDetails(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags)
{
	ComPtr<ICorDebugValue> exRawVal;
	ComPtr<ICorDebugReferenceValue> exRawRef;
	ComPtr<ICorDebugValue> exRaw;
	ComPtr<ICorDebugObjectValue> current_exception;
	ComPtr<ICorDebugClass> exClass;
	mdTypeDef exToken;

	// Get exception metadata token.
	if ((Thread.GetCurrentException(&exRawVal) == S_OK)
		&& (exRawVal.As(&exRawRef) == S_OK)
		&& (exRawRef->Dereference(&exRaw) == S_OK)
		&& (exRaw.As(&current_exception) == S_OK)
		&& (current_exception->GetClass(&exClass) == S_OK)
		&& (exClass->GetToken(&exToken) == S_OK))
	{
		// Try to resolve NullReference (or any other exception - extend).
		static mdToken NullReferenceToken = mdTokenNil;

		if (NullReferenceToken == mdTokenNil)
		{
			ComPtr<ICorDebugModule> exModule;
			ComPtr<IMetaDataImport> exModMeta;
			if ((exClass->GetModule(&exModule) == S_OK)
				&& (exModule->GetMetaDataInterface(IID_IMetaDataImport, &exModMeta) == S_OK))
			{
				wchar_t className[2048];
				ULONG classNameLen;
				DWORD typeDefFlags;
				mdToken baseClass;
				if (exModMeta->GetTypeDefProps(exToken, className, sizeof(className), &classNameLen, &typeDefFlags, &baseClass) == S_OK)
				{
					if (wcscmp(className, L"System.NullReferenceException") == 0)
					{
						NullReferenceToken = exToken;
					}
				}
			}
		}

		// Null reference handling.
		if (exToken == NullReferenceToken)
		{
			// Find IP.		
			BOOL hasIL = false;

			ComPtr<ICorDebugILFrame> ILFrame;
			ComPtr<ICorDebugCode> ILCode;
			ULONG32 ILip;
			CorDebugMappingResult ILMappingType;
			if ((Frame.QueryInterface(IID_ICorDebugILFrame, &ILFrame) == S_OK)
				&& (ILFrame->GetCode(&ILCode) == S_OK)
				&& (ILFrame->GetIP(&ILip, &ILMappingType) == S_OK))
			{
				hasIL = true;
			}

			// Get the IL code.
			byte ILBuffer[5000];
			ULONG32 ILBytes = 0;
			if (hasIL && (GetCode(ILCode.Get(), 5000, ILBuffer, &ILBytes) == S_OK))
			{
				// somehow the returned IL from getcode is always shifted by 1.
				auto ilPtr = nOffset + 1;

				if (ILMappingType == CorDebugMappingResult::MAPPING_APPROXIMATE)
				{
					auto i = ilPtr - 1;
					do
					{
						i++;
					} while (i < ILBytes && !CEE_CANNULLREF(ILBuffer[i]));

					// final check.
					if (i < ILBytes && CEE_CANNULLREF(ILBuffer[i]))
						ilPtr = i;
				}

				
				auto opcode = ILBuffer[ilPtr];
				mdToken typeToken = CEE_HASTYPETOKEN(opcode) ? *reinterpret_cast<mdToken*>(ILBuffer + ilPtr + CEE_OPBYTES(opcode)) : mdTokenNil;
				
				ComPtr<ICorDebugFunction> pFunction;
				mdMethodDef functionToken;
				if ((Frame.GetFunction(&pFunction) == S_OK)
					&& (pFunction->GetToken(&functionToken) == S_OK))
				{
					MetaInfo->ResolveTokenAndAddToCache(functionToken, pFunction.Get());
				}			

				wchar_t* functionName = MetaInfo->GetName(functionToken);

				TRACE(L"IL found: %u locations, dereferencing operation at %u - opcode %#x\n", ILBytes, nOffset, opcode);
				//LOG(L"IL found: %u locations, dereferencing operation at %u - opcode %#x\n", ILBytes, nOffset, opcode);

				switch (opcode)
				{
				case CEE_CALLVIRT:
				{
					MetaInfo->ResolveTokenAndAddToCache(typeToken, pFunction.Get());

					TRACE(L"Attempted to call %s on an uninitialized type. In %s IL %u/%u (reported/actual).\n", MetaInfo->GetName(typeToken), functionName, nOffset, ilPtr - 1);
					LOG(L"Attempted to call %s on an uninitialized type. In %s IL %u/%u (reported/actual).\n", MetaInfo->GetName(typeToken), functionName, nOffset, ilPtr - 1);
					break;
				}
				case CEE_THROW:
					TRACE(L"Attempted to throw an uninitialized exception object. In %s IL %u/%u (reported/actual).\n", functionName, nOffset, ilPtr - 1);
					LOG(L"Attempted to throw an uninitialized exception object. In %s IL %u/%u (reported/actual).\n", functionName, nOffset, ilPtr - 1);
					break;
				case CEE_LDLEN:
					TRACE(L"Attempted to get the length of an uninitialized array. In %s IL %u/%u (reported/actual).\n", functionName, nOffset, ilPtr - 1);
					LOG(L"Attempted to get the length of an uninitialized array. In %s IL %u/%u (reported/actual).\n", functionName, nOffset, ilPtr - 1);
					break;
				case CEE_UNBOXANY:
				{
					MetaInfo->ResolveTokenAndAddToCache(typeToken, pFunction.Get());

					TRACE(L"Attempted to cast/unbox a value/reference type of type %s using an uninitialized address. In %s IL %u/%u (reported/actual).\n", MetaInfo->GetName(typeToken), functionName, nOffset, ilPtr - 1);
					LOG(L"Attempted to cast/unbox a value/reference type of type %s using an uninitialized address. In %s IL %u/%u (reported/actual).\n", MetaInfo->GetName(typeToken), functionName, nOffset, ilPtr - 1);
					break;
				}
				case CEE_LDFLD:
				case CEE_LDFLDA:
				case CEE_STFLD:
				{
					MetaInfo->ResolveTokenAndAddToCache(typeToken, pFunction.Get());

					// Todo resolve type/field.
					auto store = opcode == CEE_STFLD;
					TRACE(L"Attempted to %s non-static field %s %s an uninitialized type. In %s IL %u/%u (reported/actual).\n", store ? L"store" : L"load", MetaInfo->GetName(typeToken), store ? L"in" : L"from", functionName, nOffset, ilPtr - 1);
					LOG(L"Attempted to %s non-static field %s %s an uninitialized type. In %s IL %u/%u (reported/actual).\n", store ? L"store" : L"load", MetaInfo->GetName(typeToken), store ? L"in" : L"from", functionName, nOffset, ilPtr - 1);
					break;
				}
				case CEE_LDELEM:
				case CEE_LDELEMA:
				case CEE_STELEM:
				{
					MetaInfo->ResolveTokenAndAddToCache(typeToken, pFunction.Get());

					auto store = opcode == CEE_STELEM;
					TRACE(L"Attempted to %s elements of type %s %s an uninitialized array. In %s IL %u/%u (reported/actual).\n", store ? L"store" : L"load", MetaInfo->GetName(typeToken), store ? L"in" : L"from", functionName, nOffset, ilPtr - 1);
					LOG(L"Attempted to %s elements of type %s %s an uninitialized array. In %s IL %u/%u (reported/actual).\n", store ? L"store" : L"load", MetaInfo->GetName(typeToken), store ? L"in" : L"from", functionName, nOffset, ilPtr - 1);
					break;
				}
				default:
					if (opcode >= CEE_LDELEM_I1 && opcode <= CEE_STELEM_REF)
					{
						auto typeStr = OpcodeResolver::BuiltInTypeStringByOpcode(opcode);
						auto store = opcode >= CEE_STELEM_I;

						TRACE(L"Attempted to %s elements of type %s %s an uninitialized array. In %s IL %u/%u (reported/actual).\n", store ? L"store" : L"load", typeStr, store ? L"in" : L"from", functionName, nOffset, ilPtr - 1);
						LOG(L"Attempted to %s elements of type %s %s an uninitialized array. In %s IL %u/%u (reported/actual).\n", store ? L"store" : L"load", typeStr, store ? L"in" : L"from", functionName, nOffset, ilPtr - 1);
						break;
					}
					if (opcode >= CEE_LDIND_I1 && opcode <= CEE_LDIND_REF)
					{
						auto typeStr = OpcodeResolver::BuiltInTypeStringByOpcode(opcode);

						TRACE(L"Attempted to load elements of type %s indirectly from an illegal address. In %s IL %u/%u (reported/actual).\n", typeStr, functionName, nOffset, ilPtr - 1);
						LOG(L"Attempted to load elements of type %s indirectly from an illegal address. In %s IL %u/%u (reported/actual).\n", typeStr, functionName, nOffset, ilPtr - 1);
						break;
					}
					if (opcode >= CEE_STIND_REF && opcode <= CEE_STIND_R8)
					{
						auto typeStr = OpcodeResolver::BuiltInTypeStringByOpcode(opcode);

						TRACE(L"Attempted to store elements of type %s indirectly to a misaligned or illegal address. In %s IL %u/%u (reported/actual).\n", typeStr, functionName, nOffset, ilPtr - 1);
						LOG(L"Attempted to store elements of type %s indirectly to a misaligned or illegal address. In %s IL %u/%u (reported/actual).\n", typeStr, functionName, nOffset, ilPtr - 1);
						break;
					}


					if (ILMappingType == CorDebugMappingResult::MAPPING_APPROXIMATE)
					{
						TRACE(L"Unable to get NullReference details - approximate mapping. In %s IL %u (reported).\n", functionName, nOffset);
						LOG(L"Unable to get NullReference details - approximate mapping. In %s IL %u (reported).\n", functionName, nOffset);
					}
					else
					{
						TRACE(L"Unable to get NullReference details - unknown opcode %#x. In %s IL %u/%u (reported/actual).\n", opcode, functionName, nOffset, ilPtr - 1);
						LOG(L"Unable to get NullReference details - unknown opcode %#x\n. In %s IL %u/%u (reported/actual)", opcode, functionName, nOffset, ilPtr - 1);
					}

					break;
				}
			}
			else
			{
				TRACE(L"Null reference thrown in frame with no IL code.\n");
				LOG(L"Null reference thrown in frame with no IL code.\n");
			}
		}
		else
		{
			// No NullReference, other types here.
		}
	}
	else
	{
		TRACE(L"Failed to get exception metadata.\n");
		LOG(L"Failed to get exception metadata.\n");
	}
}

//need to register this as methods can be exited by exception as well as regular returns
void Debugger::OnException(ICorDebugAppDomain &AppDomain, ICorDebugThread &Thread, ICorDebugFrame &Frame, ULONG32 nOffset, CorDebugExceptionCallbackType dwEventType, DWORD dwFlags)
{
	DWORD threadId = 0;
	mdMethodDef methodToken = 0;
	if ((Thread.GetID(&threadId) != S_OK) || (Frame.GetFunctionToken(&methodToken) != S_OK)) return;

	switch (dwEventType)
	{
	case CorDebugExceptionCallbackType::DEBUG_EXCEPTION_FIRST_CHANCE:
	{
		// Try to do more rich logging of some special (CLR thrown) exceptions.
		LogExceptionDetails(AppDomain, Thread, Frame, nOffset, dwEventType, dwFlags);

		//is the method in which the exception was thrown in our pending timer list ? (is it being monitored)
		auto hash = methodToken ^ threadId;
		auto timerSearch = pendingTimers.find(hash);
		if (timerSearch == pendingTimers.end()) return;

		//yes, register the exception
		knownExceptions.push_back(KnownException{ methodToken, threadId });
		return;
	}
	case CorDebugExceptionCallbackType::DEBUG_EXCEPTION_CATCH_HANDLER_FOUND:
	{
		//check in known exceptions if this handler is in the same method as it was thrown
		for (auto keIt = knownExceptions.begin(); keIt != knownExceptions.end(); ++keIt)
		{
			auto thisKe = *keIt;
			if (thisKe.thrownInMethod == methodToken && thisKe.thrownOnThreadId == threadId)
			{
				//found, remove it, this is handled and didn't lead to exit from method
				knownExceptions.erase(keIt);
				return;
			}
		}

		//not found yet, so it must be this exception was handled in another frame			
		//assumption: there can only be 1 known exception per thread. throwing while searching for handler is impossible, would mean the CLR throws

		//find the known exception for this thread
		for (auto keIt = knownExceptions.begin(); keIt != knownExceptions.end(); ++keIt)
		{
			auto thisKe = *keIt;
			if (thisKe.thrownOnThreadId != threadId) continue;

			//now find the pending timer for the method it was originally thrown in
			auto hash = thisKe.thrownInMethod ^ thisKe.thrownOnThreadId;
			auto timerSearch = pendingTimers.find(hash);

			//not found, exit handling (this is bad)
			if (timerSearch == pendingTimers.end()) return;

			//register abnormal exit
			InterlockedIncrement(&(timerSearch->second.method->methodExitThroughException));

			//remove from pending and knownexceptions
			pendingTimers.erase(timerSearch);
			knownExceptions.erase(keIt);
			return;
		}
		return;
	}
	case CorDebugExceptionCallbackType::DEBUG_EXCEPTION_UNHANDLED:
	{
		//this will lead to failure, just log
		TRACE(L"Unhandled exception on thread %u\n", threadId);
		LOG(L"Unhandled exception on thread %u\n", threadId);
		return;
	}
	case CorDebugExceptionCallbackType::DEBUG_EXCEPTION_USER_FIRST_CHANCE:
	default:
		return;
	}
}

void Debugger::GetBPStats(vector<shared_ptr<BreakpointInfo>> &BPStats)
{
	for (auto bpIt = managedBPs.begin(); bpIt != managedBPs.end(); ++bpIt)
	{
		BPStats.push_back((*bpIt).second);
	}
}

HRESULT Debugger::DumpFieldValue(const wchar_t* fieldSig, ComPtr<ICorDebugValue> &pVal)
{
	ULONG32 stringLen = 300000;
	auto staticAsString = unique_ptr<wchar_t[]>(new wchar_t[stringLen]);

	if (TryGetStringFromObject(pVal, staticAsString.get(), stringLen, &stringLen) == S_OK)
	{
		TRACE(L"Field: %s Value: %s\n", fieldSig, staticAsString.get());
		LOG(L"Field: %s, Value: %s\n", fieldSig, staticAsString.get());

		return S_OK;
	}
	else
	{
		TRACE(L"Failed to read field %s\n", fieldSig);

		return E_FAIL;
	}
}

HRESULT Debugger::TryGetStringFromObject(ComPtr<ICorDebugValue> &pObj, wchar_t *stringValue, ULONG32 maxChars, ULONG32 *actualChars)
{
	//get the element type
	CorElementType eType;
	if (pObj->GetType(&eType) != S_OK) return E_FAIL;

	//address and size of element
	CORDB_ADDRESS address = NULL;
	ULONG32 size;
	if ((pObj->GetAddress(&address) != S_OK) || (pObj->GetSize(&size) != S_OK)) return E_FAIL;

	//read the element in our memory
	ULONG32 readBytes;
	auto buffer = unique_ptr<BYTE[]>(new BYTE[size]);
	if (ReadMemory(DebugClientManaged->CorProcess(), address, buffer.get(), size, &readBytes) != S_OK) return E_FAIL;

	//element dependent logic
	HRESULT retval = E_NOTIMPL;
	switch (eType)
	{
	case ELEMENT_TYPE_STRING:
	{
#if __WIN64
		auto derefAddr = (CORDB_ADDRESS)load<__int64>(buffer);
#else
		auto derefAddr = (CORDB_ADDRESS)load<__int32>(buffer);
#endif
		if (derefAddr == NULL)
		{
			VERIFY(wcscpy_s(stringValue, maxChars, L"<null string>") == 0);
			retval = S_OK;
			break;
		}

		COR_TYPEID stringTypeId;
		COR_TYPE_LAYOUT stringLayout;
		COR_ARRAY_LAYOUT stringArrayLayout;

		auto process5 = DebugClientManaged->CorProcess5();

		if ((process5->GetTypeID(derefAddr, &stringTypeId) == S_OK)
			&& (process5->GetTypeLayout(stringTypeId, &stringLayout) == S_OK)
			&& (stringLayout.type == ELEMENT_TYPE_STRING)
			&& (process5->GetArrayLayout(stringTypeId, &stringArrayLayout) == S_OK))
		{
			auto string_length_addr = derefAddr + stringArrayLayout.countOffset;
			auto string_char_data_addr = derefAddr + stringArrayLayout.firstElementOffset;

			ULONG32 length;
			ULONG32 dBytesRead;
			if ((ReadMemory(nullptr, string_length_addr, (BYTE*)&length, 4, &dBytesRead) == S_OK) && (dBytesRead == 4))
			{
				auto toRead = (length + 1)*stringArrayLayout.elementSize;

				if ((ReadMemory(nullptr, string_char_data_addr, (BYTE*)stringValue, toRead, &dBytesRead) == S_OK) && (toRead == dBytesRead))
				{
					retval = S_OK;
				}
			}
		}
		break;
	}
	case ELEMENT_TYPE_CHAR:
	{
		auto wstr = wstring(1, load<wchar_t>(buffer));
		auto cstr = wstr.c_str();
		VERIFY(wcscpy_s(stringValue, maxChars, cstr) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_BOOLEAN:
	{
		auto boolval = buffer[0] > 0;
		VERIFY(wcscpy_s(stringValue, maxChars, boolval ? L"true" : L"false") == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_I1:
	{
		auto val = load<signed __int8>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_U1:
	{
		auto val = load<unsigned __int8>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_I2:
	{
		auto val = load<signed __int16>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_U2:
	{
		auto val = load<unsigned __int16>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_I4:
	{
		auto val = load<signed __int32>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_U4:
	{
		auto val = load<unsigned __int32>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_I8:
	{
		auto val = load<signed __int64>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_U8:
	{
		auto val = load<unsigned __int64>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_R4:
	{
		auto val = load<float>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_R8:
	{
		auto val = load<double>(buffer);
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_VOID:			//void (a field never has this type)
		VERIFY(wcscpy_s(stringValue, maxChars, L"<void>") == 0);
		retval = S_OK;
		break;
	case ELEMENT_TYPE_PTR:
	{								// PTR <type>
		auto typeToken = load<mdTypeDef>(buffer);
		auto wstr = to_wstring(typeToken);
		VERIFY(wcscpy_s(stringValue, maxChars, L"PTR(*) mdTypeDef=") == 0);
		VERIFY(wcscat_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_BYREF:
	{								// BYREF <type>
		auto typeToken = load<mdTypeDef>(buffer);
		auto wstr = to_wstring(typeToken);
		wcscpy_s(stringValue, maxChars, L"BYREF(&) mdTypeDef=");
		wcscat_s(stringValue, maxChars, wstr.c_str());
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_VALUETYPE:
	{
		// VALUETYPE <class Token>
		auto classToken = load<mdTypeDef>(buffer);
		auto wstr = to_wstring(classToken);
		VERIFY(wcscpy_s(stringValue, maxChars, L"VALUETYPE mdTypeDef=") == 0);
		VERIFY(wcscat_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_CLASS:
	{								// CLASS <class Token>
		if (DereferenceIfPossible(&pObj) == S_OK)
		{
			VERIFY(TryGetStringFromObject(pObj, stringValue, maxChars, actualChars) == S_OK);
		}
		else
		{
			//todo resolve class
			auto classToken = load<mdTypeDef>(buffer);
			auto wstr = to_wstring(classToken);
			VERIFY(wcscpy_s(stringValue, maxChars, L"CLASS mdTypeDef=") == 0);
			VERIFY(wcscat_s(stringValue, maxChars, wstr.c_str()) == 0);
		}
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_ARRAY:
	{
		// MDARRAY <type> <rank> <bcount> <bound1> ... <lbcount> <lb1> ...
		auto arrayToken = load<mdTypeDef>(buffer);
		auto wstr = to_wstring(arrayToken);
		VERIFY(wcscpy_s(stringValue, maxChars, L"MDARRAY mdTypeDef=") == 0);
		VERIFY(wcscat_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_I:
	{								// native integer size (System.IntPtr)
#if __WIN64
		auto val = load<signed __int64>(buffer);
#else
		auto val = load<signed __int32>(buffer);
#endif
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_U:
	{								// native unsigned integer size (System.UIntPtr)
#if __WIN64
		auto val = load<unsigned __int64>(buffer);
#else
		auto val = load<unsigned __int32>(buffer);
#endif
		auto wstr = to_wstring(val);
		VERIFY(wcscpy_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_FNPTR:
		// FNPTR <complete sig for the function including calling convention>
		VERIFY(wcscpy_s(stringValue, maxChars, L"<FNPTR>") == 0);
		retval = S_OK;
		break;
	case ELEMENT_TYPE_OBJECT:		// Shortcut for System.Object
		VERIFY(wcscpy_s(stringValue, maxChars, L"System.Object") == 0);
		retval = S_OK;
		break;
	case ELEMENT_TYPE_SZARRAY:
	{
		// Shortcut for single dimension zero lower bound array, SZARRAY <type>
		auto arrayToken = load<mdTypeDef>(buffer);
		auto wstr = to_wstring(arrayToken);

		VERIFY(wcscpy_s(stringValue, maxChars, L"[] mdTypeDef=") == 0);
		VERIFY(wcscat_s(stringValue, maxChars, wstr.c_str()) == 0);
		retval = S_OK;
		break;
	}
	case ELEMENT_TYPE_SENTINEL:		// sentinel for varargs
		VERIFY(wcscpy_s(stringValue, maxChars, L"<vararg sentinel>") == 0);
		retval = S_OK;
		break;
	case ELEMENT_TYPE_PINNED:		// local var referencing a pinned object
		//todo resolve the reference
		VERIFY(wcscpy_s(stringValue, maxChars, L"<local var referencing a pinned object>") == 0);
		retval = S_OK;
		break;
	case ELEMENT_TYPE_END:			// internal use
	case ELEMENT_TYPE_INTERNAL:     // INTERNAL <typehandle>
		VERIFY(wcscpy_s(stringValue, maxChars, L"<internal use type>") == 0);
		retval = S_OK;
		break;
	case ELEMENT_TYPE_MVAR:		    // a method type variable MVAR <number>
	case ELEMENT_TYPE_CMOD_REQD:    // required C modifier : E_T_CMOD_REQD <mdTypeRef/mdTypeDef>
	case ELEMENT_TYPE_CMOD_OPT:     // optional C modifier : E_T_CMOD_OPT <mdTypeRef/mdTypeDef>
	case ELEMENT_TYPE_VAR:			// a class type variable VAR <number>
	case ELEMENT_TYPE_GENERICINST:  // GENERICINST <generic type> <argCnt> <arg1> ... <argn>
	case ELEMENT_TYPE_TYPEDBYREF:   // TYPEDREF  (it takes no args) a typed referece to some other type
	default:
		VERIFY(wcscpy_s(stringValue, maxChars, L"<Unknown element type>") == 0);
		retval = S_OK;
		break;
	}

	return retval;
}

HRESULT Debugger::ReadMemory(ICorDebugProcess *pProcess, CORDB_ADDRESS address, BYTE *pBuffer, ULONG32 bytesRequested, ULONG32 *pBytesRead)
{
	ASSERT(pBuffer);
	ASSERT(pBytesRead);

	//get a CorDebugProcess
	auto pProc = pProcess;
	if (!pProc)
	{
		ASSERT(DebugClientManaged);
		pProc = DebugClientManaged->CorProcess();
	}

	ASSERT(pProc);
	return pProc->ReadMemory(address, bytesRequested, pBuffer, (SIZE_T*)pBytesRead);
}

HRESULT Debugger::DereferenceIfPossible(ICorDebugValue** pVal)
{
	ASSERT(pVal);
	ASSERT(*pVal);

	ComPtr<ICorDebugValue> origObj = *pVal;
	ComPtr<ICorDebugReferenceValue> refVal;

	if (origObj.As(&refVal) != S_OK) return E_FAIL;

	//check null
	BOOL isNull;
	if ((refVal->IsNull(&isNull) != S_OK) || isNull) return E_FAIL;

	//value is reference, and not null, overwrite input
	VERIFY(refVal->Dereference(pVal) == S_OK); //adds 1 ref to ICorDebugValue

	return S_OK;
}

HRESULT Debugger::GetConstValue(DWORD rawType, UVCP_CONSTANT fieldValueLocation, ULONG fieldValueSize, wchar_t **constAsString)
{
	auto type = static_cast<CorElementType>(rawType);
	auto fieldValue = const_cast<void*>(static_cast<const void*>(fieldValueLocation));
	ASSERT(fieldValue);

	wstring wstr;

	switch (type)
	{
	case ELEMENT_TYPE_BOOLEAN:
	{
		auto boolval = load<unsigned char>(fieldValue) > 0;
		wstr = wstring(boolval ? L"true" : L"false");
		break;
	}
	case ELEMENT_TYPE_CHAR:
		wstr = wstring(1, load<wchar_t>(fieldValue));
		break;
	case ELEMENT_TYPE_I1:
		wstr = to_wstring(load<signed __int8>(fieldValue));
		break;
	case ELEMENT_TYPE_U1:
		wstr = to_wstring(load<unsigned __int8>(fieldValue));
		break;
	case ELEMENT_TYPE_I2:
		wstr = to_wstring(load<signed __int16>(fieldValue));
		break;
	case ELEMENT_TYPE_U2:
		wstr = to_wstring(load<unsigned __int16>(fieldValue));
		break;
	case ELEMENT_TYPE_I4:
		wstr = to_wstring(load<signed __int32>(fieldValue));
		break;
	case ELEMENT_TYPE_U4:
		wstr = to_wstring(load<unsigned __int32>(fieldValue));
		break;
	case ELEMENT_TYPE_I8:
		wstr = to_wstring(load<signed __int64>(fieldValue));
		break;
	case ELEMENT_TYPE_U8:
		wstr = to_wstring(load<unsigned __int64>(fieldValue));
		break;
	case ELEMENT_TYPE_R4:
		wstr = to_wstring(load<float>(fieldValue));
		break;
	case ELEMENT_TYPE_R8:
		wstr = to_wstring(load<double>(fieldValue));
		break;
	case ELEMENT_TYPE_STRING:
		//tested: null string will be returned as ELEMENT_TYPE_CLASS, so anything that ends up here will be an actual string
		wstr = wstring(reinterpret_cast<wchar_t*>(const_cast<void*>(fieldValue)), fieldValueSize);
		break;
	default:
		//any non-simple reference type, const value can only be null
		wstr = wstring(L"null");
		break;
	}

	//copy wstring to const buffer
	auto len = wstr.size();
	*constAsString = new wchar_t[len + 1];
	VERIFY(wcscpy_s(*constAsString, len + 1, wstr.c_str()) == 0);

	return S_OK;
}

MemoryInfo* Debugger::GetMemoryInfo()
{
	auto memInfo = new MemoryInfo(this->DebugClientManaged->CorProcess());
	memInfo->Init();

	return memInfo;
}
