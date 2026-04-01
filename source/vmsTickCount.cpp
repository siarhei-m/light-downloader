/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "vmsTickCount.h"

vmsCriticalSection* vmsTickCount::m_pcsMyGetTickCount64 = new vmsCriticalSection;

vmsTickCount::vmsTickCount(void) {}

vmsTickCount::~vmsTickCount(void) {}

UINT64 vmsTickCount::GetTickCount64(void)
{
	return ::GetTickCount64();
}

UINT64 WINAPI vmsTickCount::myGetTickCount64(void)
{
	return ::GetTickCount64();
}
