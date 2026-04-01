/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#pragma once
#include "stdafx.h"
#include "tstring.h"
#include <fstream>

class vmsLogger
{
	static tstring GetTimeStamp();

  public:
	static void WriteLog(const tstring& message);
};
