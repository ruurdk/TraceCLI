// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers



// TODO: reference additional headers your program requires here
#include <Windows.h>

#include <cor.h>
#include <cordebug.h>
#include <metahost.h>
#pragma comment(lib,"mscoree.lib")
#pragma comment(lib,"dbgeng.lib")

#include <Psapi.h>
#include <DbgEng.h>

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <set>

using std::unique_ptr;
using std::shared_ptr;
using std::weak_ptr;

using std::vector;
using std::map;
using std::function;
using std::set;

#include <wrl.h>

using Microsoft::WRL::ComPtr;

#include "../Shared/tracing.h"
#include "../Shared/Logger.h"

#include "DebugCore.h"