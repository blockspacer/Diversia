/*
-----------------------------------------------------------------------------
Copyright (c) 2008-2010 Diversia

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#ifndef DIVERSIA_SHARED_STABLEHEADERS_H
#define DIVERSIA_SHARED_STABLEHEADERS_H

// Platform
#include "Util/Build/Platform.h"
#include "Util/Build/StdHeaders.h"
#include "Shared/Platform/Forward.h"
#include "Shared/Platform/Prerequisites.h"

// Framework
#include "Object/ObjectIncludes.h"
#include "Util/UtilIncludes.h"

// Libraries
// RakNet
#include <RakNet/BitStream.h>
#include <RakNet/RakString.h>
#include <RakNet/ReplicaManager3.h>
#include <RakNet/NetworkIDManager.h>

// Lua
#include <camp-lua/lua.hpp>

// Framework
#include "Shared/Communication/BitStream.h"

#endif // DIVERSIA_SHARED_STABLEHEADERS_H