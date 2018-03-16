#include "FileConfigReader.h"
#include "..\Shared\tracing.h"


FileConfigReader::FileConfigReader(const wchar_t *fileName) : config(nullptr)
{
	ParseConfig(fileName);
}


FileConfigReader::~FileConfigReader()
{
}

bool FileConfigReader::ParseConfig(const wchar_t *fileName)
{
	ASSERT(fileName);

	IStream *fileStream;
	if (SHCreateStreamOnFile(fileName, STGM_READ, &fileStream) != S_OK) return false;

	bool retval = false;

	bool anyFilterWithFields = false;
	OPMODE mode = OPMODE_NONE;

	ComPtr<IXmlReader> xmlReader;
	if ((CreateXmlReader(_uuidof(IXmlReader), &xmlReader, nullptr) == S_OK) 
		&& (xmlReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit) == S_OK)
		&& (xmlReader->SetInput(fileStream) == S_OK))
	{
		auto newConfig = shared_ptr<Config>(new Config{});

		//read it
		auto depth = 0;
		XmlNodeType node;
		while (xmlReader->Read(&node) == S_OK)
		{
			switch (node)
			{
			case XmlNodeType_Element:
			{			
				const wchar_t *localName;
				UINT localNameLen;
				if (!((xmlReader->GetLocalName(&localName, &localNameLen) == S_OK) && (localNameLen) && (localName))) break;
			
				depth++;

				if ((wcscmp(L"Config", localName) == 0) && (depth == 1))
				{					
					//found config element
					//todo: operating mode, outfile
					UINT numAttributes = 0;
					if ((xmlReader->GetAttributeCount(&numAttributes) == S_OK) && numAttributes && (xmlReader->MoveToFirstAttribute() == S_OK))
					{
						const wchar_t *localAttrName;
						const wchar_t *localAttrValue;
						UINT localAttrNameLen;
						UINT localAttrValueLen;
						do
						{
							if ((xmlReader->GetQualifiedName(&localAttrName, &localAttrNameLen) == S_OK) && (xmlReader->GetValue(&localAttrValue, &localAttrValueLen) == S_OK))
							{
								if ((wcscmp(L"timings", localAttrName) == 0) && (wcscmp(L"1", localAttrValue) == 0)) mode |= OPMODE_TIMINGS;
								if ((wcscmp(L"stats", localAttrName) == 0) && (wcscmp(L"1", localAttrValue) == 0)) mode |= OPMODE_STATS;
								if (wcscmp(L"outputfile", localAttrName) == 0)
								{
									auto outfile = new wchar_t[localAttrValueLen + 1];
									wcscpy_s(outfile, localAttrValueLen + 1, localAttrValue);
									newConfig->OutfileName = outfile;
								}
							}
						} while (xmlReader->MoveToNextAttribute() == S_OK);
					}
				}
				else if ((wcscmp(L"Filter", localName) == 0) && (depth == 2))
				{
					//found filter element
					auto newFilter = shared_ptr<BPFilter>(new BPFilter{});

					UINT numAttributes = 0;
					if ((xmlReader->GetAttributeCount(&numAttributes) == S_OK) && numAttributes && (xmlReader->MoveToFirstAttribute() == S_OK))
					{
						const wchar_t *localAttrName;
						const wchar_t *localAttrValue;
						UINT localAttrNameLen;
						UINT localAttrValueLen;
						do 
						{
							if ((xmlReader->GetQualifiedName(&localAttrName, &localAttrNameLen) == S_OK) && (xmlReader->GetValue(&localAttrValue, &localAttrValueLen) == S_OK))
							{
								if (wcscmp(L"namespace", localAttrName) == 0) 
								{
									auto namespaceFilt = new wchar_t[localAttrValueLen + 1];
									wcscpy_s(namespaceFilt, localAttrValueLen + 1, localAttrValue);
									newFilter->namespaceFilter = namespaceFilt;
								}
								if (wcscmp(L"class", localAttrName) == 0)
								{
									auto classFilt = new wchar_t[localAttrValueLen + 1];
									wcscpy_s(classFilt, localAttrValueLen + 1, localAttrValue);
									newFilter->classFilter = classFilt;
								}
								if (wcscmp(L"method", localAttrName) == 0)
								{
									auto methodFilt = new wchar_t[localAttrValueLen + 1];
									wcscpy_s(methodFilt, localAttrValueLen + 1, localAttrValue);
									newFilter->methodFilter = methodFilt;
								}
								if (wcscmp(L"fields", localAttrName) == 0) 
								{	
									auto wstr = std::wstring(localAttrValue);
									std::string df;
									df.assign(wstr.begin(), wstr.end());
									
									vector<std::string> parsedFields;
									split(df, ',', parsedFields);

									for (auto fieldIt = parsedFields.begin(); fieldIt != parsedFields.end(); ++fieldIt)
									{
										auto thisField = *fieldIt;
										//skip if empty
										if (!thisField.size()) continue;

										//convert to wstring
										std::wstring ws;
										ws.assign(thisField.begin(), thisField.end());

										//push on heap
										auto asCStr = ws.c_str();
										auto newHeapStr = new wchar_t[wcslen(asCStr) + 1];
										wcscpy_s(newHeapStr, wcslen(asCStr) + 1, asCStr);

										newFilter->fieldsToDump.push_back(newHeapStr);

										anyFilterWithFields = true;
									}
								}
							}
						} while (xmlReader->MoveToNextAttribute() == S_OK);
						
						newConfig->Breakpoints.push_back(newFilter);
					}
				}

				if (xmlReader->IsEmptyElement()) depth--;

				break;
			}
			case XmlNodeType_EndElement:
				depth--;
				break;						
			default:
				break;
			}
		}

		if (anyFilterWithFields) mode |= OPMODE_FIELDS;
		newConfig->OperatingMode = mode;

		config = newConfig;
	}

	if (fileStream) fileStream->Release();

	return retval;
}

Config* FileConfigReader::ProvideConfig()
{
	return config.get();
}

const wchar_t* FileConfigReader::HelpString()
{
	return helpString;
}