#pragma once
class MetaHelpers
{
public:
	MetaHelpers(ICorDebugProcess *pProcess);
	~MetaHelpers();

	// Directly add and get from cache.
	void AddToCache(const mdTypeDef token, const shared_ptr<wchar_t> name)
	{
		tokenCache[token] = name;
	}
	wchar_t * GetName(const mdToken &token);

	// Add to cache but let MetaHelpers resolve the name.
	void ResolveTokenAndAddToCache(mdToken refToken, ICorDebugFunction *enclosingMethod);
private:
	ComPtr<ICorDebugProcess> pProcess;
	
	// Lookup table for typedef and methoddef. Needed for resolution of tokens in breakpoints
	map<mdToken, shared_ptr<wchar_t>> tokenCache;	

	shared_ptr<wchar_t> GetTypeDefName(mdToken token, IMetaDataImport* modmeta);
	shared_ptr<wchar_t> GetTypeRefName(mdToken token, IMetaDataImport* modmeta);
	shared_ptr<wchar_t> GetMemberRefName(mdToken token, IMetaDataImport* modmeta);
	shared_ptr<wchar_t> GetMethodDefName(mdToken token, IMetaDataImport* modmeta);
	shared_ptr<wchar_t> GetFieldDefName(mdToken token, IMetaDataImport* modmeta);
};