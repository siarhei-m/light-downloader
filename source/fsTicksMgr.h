/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#if !defined(AFX_FSTICKSMGR_H__53B4638E_4C2E_46B2_9948_E8553BFC4693__INCLUDED_)
#define AFX_FSTICKSMGR_H__53B4638E_4C2E_46B2_9948_E8553BFC4693__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

class fsTicksMgr
{
  public:
	ULONGLONG operator-(fsTicksMgr& ticks);

	void Now();

	fsTicksMgr();
	virtual ~fsTicksMgr();

	ULONGLONG m_dwTicks;

	void operator+=(ULONGLONG dwTicks) { m_dwTicks += dwTicks; }

	bool operator<(const fsTicksMgr& obj) const { return m_dwTicks < obj.m_dwTicks; }
	bool operator<=(const fsTicksMgr& obj) const { return m_dwTicks <= obj.m_dwTicks; }
};

#endif
