#include "Config.h"

#pragma once

class SQLConfigProvider : public IConfigProvider
{
public:	
	Config* ProvideConfig() override;
	const wchar_t* HelpString() override;
};

class UnitTestConfigProvider : public IConfigProvider
{
public:
	Config* ProvideConfig() override;
	const wchar_t* HelpString() override;
};

class ADConfigProvider : public IConfigProvider
{
public:
	Config* ProvideConfig() override;
	const wchar_t* HelpString() override;
};
