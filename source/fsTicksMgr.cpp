/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "fsTicksMgr.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

fsTicksMgr::fsTicksMgr()
{
	Now();
}

fsTicksMgr::~fsTicksMgr() {}

void fsTicksMgr::Now()
{
	m_dwTicks = GetTickCount64();
}

ULONGLONG fsTicksMgr::operator-(fsTicksMgr& ticks)
{
	return m_dwTicks - ticks.m_dwTicks;
}
