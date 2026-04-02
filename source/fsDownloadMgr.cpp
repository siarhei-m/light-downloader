/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "fsDownloadMgr.h"
#include "inetutil.h"
#include "misc.h"
#include "../hash/vmsHash.h"

#include "DownloadsWnd.h"
#include "Dlg_AER.h"
#include "Dlg_SCR.h"
#include "MyMessageBox.h"

#include "fsDownloadsMgr.h"
#include <mlang.h>
#include "vmsLogger.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif


// Helper: write std::string to buffer
static void SerializeString(const std::string& s, LPBYTE& pB)
{
	DWORD len = (DWORD)s.length();
	CopyMemory(pB, &len, sizeof(DWORD));
	pB += sizeof(DWORD);
	if (len > 0)
	{
		CopyMemory(pB, s.c_str(), len);
		pB += len;
	}
}

// Helper: read std::string from buffer
static bool DeserializeString(std::string& s, LPBYTE& pB, LPBYTE pEnd)
{
	if (pB + sizeof(DWORD) > pEnd) return false;
	DWORD len;
	CopyMemory(&len, pB, sizeof(DWORD));
	pB += sizeof(DWORD);
	if (len > 0)
	{
		if (pB + len > pEnd) return false;
		s.assign((const char*)pB, len);
		pB += len;
	}
	else
		s.clear();
	return true;
}

// Helper: calculate serialized size of a string
static DWORD SerializedStringSize(const std::string& s)
{
	return sizeof(DWORD) + (DWORD)s.length();
}

// Helper: serialize non-string fields of fsDownload_Properties
static void SerializeDP_NonString(const fsDownload_Properties& dp, LPBYTE& pB)
{
	// Write all non-string fields as a binary block
	// We write them individually to avoid std::string being in the middle
	#define WRITE_FIELD(f) CopyMemory(pB, &dp.f, sizeof(dp.f)); pB += sizeof(dp.f)
	WRITE_FIELD(wStructSize);
	WRITE_FIELD(uTrafficRestriction);
	WRITE_FIELD(uMaxAttempts);
	WRITE_FIELD(uRetriesTime);
	WRITE_FIELD(uTimeout);
	WRITE_FIELD(uSectionMinSize);
	WRITE_FIELD(uMaxSections);
	WRITE_FIELD(bRestartSpeedLow);
	WRITE_FIELD(bReserveDiskSpace);
	WRITE_FIELD(bIgnoreRestrictions);
	CopyMemory(pB, dp.aEP, sizeof(dp.aEP)); pB += sizeof(dp.aEP);
	WRITE_FIELD(enAER);
	WRITE_FIELD(enSCR);
	WRITE_FIELD(dwFlags);
	WRITE_FIELD(bCheckIntegrityWhenDone);
	WRITE_FIELD(enICFR);
	WRITE_FIELD(dwIntegrityCheckAlgorithm);
	#undef WRITE_FIELD
}

static bool DeserializeDP_NonString(fsDownload_Properties& dp, LPBYTE& pB, LPBYTE pEnd)
{
	#define READ_FIELD(f) if (pB + sizeof(dp.f) > pEnd) return false; CopyMemory(&dp.f, pB, sizeof(dp.f)); pB += sizeof(dp.f)
	READ_FIELD(wStructSize);
	READ_FIELD(uTrafficRestriction);
	READ_FIELD(uMaxAttempts);
	READ_FIELD(uRetriesTime);
	READ_FIELD(uTimeout);
	READ_FIELD(uSectionMinSize);
	READ_FIELD(uMaxSections);
	READ_FIELD(bRestartSpeedLow);
	READ_FIELD(bReserveDiskSpace);
	READ_FIELD(bIgnoreRestrictions);
	if (pB + sizeof(dp.aEP) > pEnd) return false;
	CopyMemory(dp.aEP, pB, sizeof(dp.aEP)); pB += sizeof(dp.aEP);
	READ_FIELD(enAER);
	READ_FIELD(enSCR);
	READ_FIELD(dwFlags);
	READ_FIELD(bCheckIntegrityWhenDone);
	READ_FIELD(enICFR);
	READ_FIELD(dwIntegrityCheckAlgorithm);
	#undef READ_FIELD
	return true;
}

static DWORD SerializedDP_NonStringSize()
{
	fsDownload_Properties dp;
	return sizeof(dp.wStructSize) + sizeof(dp.uTrafficRestriction) + sizeof(dp.uMaxAttempts) +
	       sizeof(dp.uRetriesTime) + sizeof(dp.uTimeout) + sizeof(dp.uSectionMinSize) +
	       sizeof(dp.uMaxSections) + sizeof(dp.bRestartSpeedLow) + sizeof(dp.bReserveDiskSpace) +
	       sizeof(dp.bIgnoreRestrictions) + sizeof(dp.aEP) + sizeof(dp.enAER) + sizeof(dp.enSCR) +
	       sizeof(dp.dwFlags) + sizeof(dp.bCheckIntegrityWhenDone) + sizeof(dp.enICFR) +
	       sizeof(dp.dwIntegrityCheckAlgorithm);
}

static void SerializeDNP_NonString(const fsDownload_NetworkProperties& dnp, LPBYTE& pB)
{
	#define WRITE_FIELD(f) CopyMemory(pB, &dnp.f, sizeof(dnp.f)); pB += sizeof(dnp.f)
	WRITE_FIELD(wRollBackSize);
	WRITE_FIELD(enAccType);
	WRITE_FIELD(enProtocol);
	WRITE_FIELD(uServerPort);
	WRITE_FIELD(bUseHttp11);
	WRITE_FIELD(bUseCookie);
	WRITE_FIELD(dwFtpFlags);
	WRITE_FIELD(enFtpTransferType);
	WRITE_FIELD(dwFlags);
	WRITE_FIELD(wLowSpeed_Factor);
	WRITE_FIELD(wLowSpeed_Duration);
	#undef WRITE_FIELD
}

static bool DeserializeDNP_NonString(fsDownload_NetworkProperties& dnp, LPBYTE& pB, LPBYTE pEnd)
{
	#define READ_FIELD(f) if (pB + sizeof(dnp.f) > pEnd) return false; CopyMemory(&dnp.f, pB, sizeof(dnp.f)); pB += sizeof(dnp.f)
	READ_FIELD(wRollBackSize);
	READ_FIELD(enAccType);
	READ_FIELD(enProtocol);
	READ_FIELD(uServerPort);
	READ_FIELD(bUseHttp11);
	READ_FIELD(bUseCookie);
	READ_FIELD(dwFtpFlags);
	READ_FIELD(enFtpTransferType);
	READ_FIELD(dwFlags);
	READ_FIELD(wLowSpeed_Factor);
	READ_FIELD(wLowSpeed_Duration);
	#undef READ_FIELD
	return true;
}

static DWORD SerializedDNP_NonStringSize()
{
	fsDownload_NetworkProperties dnp;
	return sizeof(dnp.wRollBackSize) + sizeof(dnp.enAccType) + sizeof(dnp.enProtocol) +
	       sizeof(dnp.uServerPort) + sizeof(dnp.bUseHttp11) + sizeof(dnp.bUseCookie) +
	       sizeof(dnp.dwFtpFlags) + sizeof(dnp.enFtpTransferType) + sizeof(dnp.dwFlags) +
	       sizeof(dnp.wLowSpeed_Factor) + sizeof(dnp.wLowSpeed_Duration);
}

static void SerializeDNP_Strings(const fsDownload_NetworkProperties& dnp, LPBYTE& pB)
{
	SerializeString(dnp.strAgent, pB);
	SerializeString(dnp.strPassword, pB);
	SerializeString(dnp.strPathName, pB);
	SerializeString(dnp.strProxyName, pB);
	SerializeString(dnp.strProxyPassword, pB);
	SerializeString(dnp.strProxyUserName, pB);
	SerializeString(dnp.strReferer, pB);
	SerializeString(dnp.strServerName, pB);
	SerializeString(dnp.strUserName, pB);
	SerializeString(dnp.strASCIIExts, pB);
	SerializeString(dnp.strCookies, pB);
	SerializeString(dnp.strPostData, pB);
}

static bool DeserializeDNP_Strings(fsDownload_NetworkProperties& dnp, LPBYTE& pB, LPBYTE pEnd)
{
	if (!DeserializeString(dnp.strAgent, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strPassword, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strPathName, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strProxyName, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strProxyPassword, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strProxyUserName, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strReferer, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strServerName, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strUserName, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strASCIIExts, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strCookies, pB, pEnd)) return false;
	if (!DeserializeString(dnp.strPostData, pB, pEnd)) return false;
	return true;
}

static DWORD SerializedDNP_StringsSize(const fsDownload_NetworkProperties& dnp)
{
	return SerializedStringSize(dnp.strAgent) + SerializedStringSize(dnp.strPassword) +
	       SerializedStringSize(dnp.strPathName) + SerializedStringSize(dnp.strProxyName) +
	       SerializedStringSize(dnp.strProxyPassword) + SerializedStringSize(dnp.strProxyUserName) +
	       SerializedStringSize(dnp.strReferer) + SerializedStringSize(dnp.strServerName) +
	       SerializedStringSize(dnp.strUserName) + SerializedStringSize(dnp.strASCIIExts) +
	       SerializedStringSize(dnp.strCookies) + SerializedStringSize(dnp.strPostData);
}

vmsCriticalSectionEx fsDownloadMgr::m_csRenameFile;

fsDownloadMgr::fsDownloadMgr(struct fsDownload* dld)
{
	m_iThread = 0;
	m_bThreadRunning = FALSE;
	m_dwState = DS_STOPPED;
	m_dld = dld;

	m_dldr.SetEventFunc(_DownloaderEvents, this);
	m_dp.wStructSize = sizeof(m_dp);
	fsDP_GetDefaults(&m_dp);
	m_dwState = 0;

	m_hOutFile = INVALID_HANDLE_VALUE;

	m_pfnEvents = NULL;
	m_pfnEventDesc = NULL;
	m_uNeedStartFrom = 0;
	m_dwDownloadFileFlags = DFF_NEED_INIT_FILE;
	m_bFatalError = FALSE;

	m_bCantStart = FALSE;

	m_uMirrRecalcSpeedTime = 60;

	m_tikLastMirrMeasureTime.Now();
	m_bNeedStartAgain = FALSE;
	m_bRename_CheckIfRenamed = FALSE;

	m_bKnownFileSharingSiteSupportAdjusted = false;

	m_bDontCreateNewSections = FALSE;
	m_dldr.SetManagerPersistObject(this);

	m_bFailedToCreateDestinationFile = false;
	m_bIsNotEnoughDiskSpace = false;
}

fsDownloadMgr::~fsDownloadMgr()
{
	m_dldr.SetEventFunc(NULL, NULL);

	StopDownloading();
	for (int i = 0; i < 10 * 1000 / 10 && m_iThread; i++)
	{
#ifdef ADDITIONAL_MSG_LOOPS_REQUIRED
		MSG msg;
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessage(&msg);
#endif

		Sleep(10);
	}

	CloseFile();
}

fsDownload_NetworkProperties* fsDownloadMgr::GetDNP()
{
	return m_dldr.DNP();
}

fsDownload_Properties* fsDownloadMgr::GetDP()
{
	return &m_dp;
}

fsInternetResult fsDownloadMgr::StartDownloading()
{
	if (IsRunning() || m_dldr.IsDone() || IsQueringSize()) return IR_S_FALSE;

	setStateFlagsTo(DS_NEEDSTART);
	setDirty();

	DWORD dwThread;
	m_bThreadRunning = TRUE;
	InterlockedIncrement(&m_iThread);
	CloseHandle(CreateThread(NULL, 0, _threadDownloadMgr, this, 0, &dwThread));

	return IR_SUCCESS;
}

fsInternetResult fsDownloadMgr::CreateInternetSession()
{
	// InternetAutodial (INTERNET_AUTODIAL_FORCE_ONLINE, NULL);
	return IR_SUCCESS;
}

// BOOL fsDownloadMgr::InternetAutodial(DWORD dwFlags, HWND hwndParent)
//{
//	HMODULE hDll = ::GetModuleHandle(_T("wininet.dll"));
//	if (hDll == 0) {
//		hDll = ::LoadLibrary("wininet.dll");
//	}
//
//	if (hDll == 0) {
//		return FALSE;
//	}
//
//	typedef BOOL (WINAPI *FUN)(DWORD p1, HWND p2);
//
//	FUN pfnFun = NULL;
//	pfnFun = (FUN)GetProcAddress(hDll, "InternetAutodial");
//
//	if (pfnFun == 0) {
//		return FALSE;
//	}
//
//	return pfnFun(dwFlags, hwndParent);
// }

void fsDownloadMgr::ApplyProperties()
{
	m_dldr.Set_Timeout(m_dp.uTimeout);
	m_dldr.SetRetryTime(m_dp.uRetriesTime);
	m_dldr.SetMaxReconnectionNumber(m_dp.uMaxAttempts);
	m_dldr.SetSectionMinSize(m_dp.uSectionMinSize);
	m_dldr.DontRestartIfNoRanges(m_dp.dwFlags & DPF_DONTRESTARTIFNORESUME);

	m_dldr.StopOnAccDenied(m_dp.aEP[DFE_ACCDENIED] == DFEP_STOP);
	m_dldr.StopOnFileNotFound(m_dp.aEP[DFE_NOTFOUND] == DFEP_STOP);

	if (m_dp.bIgnoreRestrictions == FALSE)
		m_dldr.LimitTraffic(m_dp.uTrafficRestriction);
	else
		m_dldr.LimitTraffic(UINT_MAX);

	if (m_dwState & DS_DOWNLOADING)
	{
		setStateFlags(DS_NEEDADDSECTION);
		setDirty();
	}
}

DWORD WINAPI fsDownloadMgr::_threadDownloadMgr(LPVOID lp)
{
	fsDownloadMgr* pThis = (fsDownloadMgr*)lp;
	BOOL bAddSection = TRUE;

	if (fsDownloadMgr::is_GlobalOffline()) fsDownloadMgr::set_GlobalOffline(FALSE);

	pThis->m_bThreadRunning = TRUE;

	BOOL bSSR = pThis->Event(DE_EXTERROR, DMEE_STARTING);

	if (bSSR == FALSE)
	{
		pThis->m_bFatalError = TRUE;
		pThis->Event(DE_EXTERROR, DMEE_FATALERROR);
	}

	fsTicksMgr tick0SpeedStart;
	tick0SpeedStart.Now();

	if (bSSR)
		while ((pThis->m_dwState & DS_DONE) == 0)
		{

			if (pThis->m_dwState & DS_NEEDSTART)
			{
				pThis->removeStateFlags(DS_NEEDSTART);
				bAddSection = TRUE;

				for (UINT i = 0; i < pThis->m_dp.uMaxAttempts; i++)
				{
					pThis->m_lastError = pThis->StartDownload();
					tick0SpeedStart.Now();

					if (pThis->m_dwState & DS_NEEDSTOP || pThis->m_lastError == IR_S_FALSE ||
					    pThis->m_lastError == IR_RANGESNOTAVAIL || pThis->m_lastError == IR_DOUBTFUL_RANGESRESPONSE)
						break;

					if (pThis->m_lastError == IR_SUCCESS)
					{
						pThis->setStateFlags(DS_DOWNLOADING);
						if (pThis->m_dldr.IsResumeSupported() == RST_NONE) pThis->Event(LS(L_NORESUME), EDT_WARNING);
						break;
					}
					else
					{
						pThis->m_bCantStart = TRUE;

						if (pThis->m_dp.uRetriesTime && i + 1 != pThis->m_dp.uMaxAttempts)
						{
							CHAR szStr[1000];
							sprintf(szStr, LS(L_PAUSESECS), pThis->m_dp.uRetriesTime / 1000);
							pThis->Event(szStr);
							if (pThis->SleepInterval() == FALSE) break;
						}
					}
				}

				pThis->m_bCantStart = FALSE;

				if ((pThis->m_dwState & DS_DOWNLOADING) == 0 && pThis->m_lastError != IR_S_FALSE)
				{
					if ((pThis->m_dwState & DS_NEEDSTOP) == 0)
					{
						pThis->Event(LS(L_DLDSTOPPED),
						             pThis->m_lastError == IR_S_FALSE ? EDT_RESPONSE_S : EDT_RESPONSE_E);
						pThis->Event(DE_EXTERROR, DMEE_FATALERROR);
						pThis->m_bFatalError = TRUE;
						pThis->setStateFlagsTo(0);
						break;
					}
				}
			}

			if (pThis->m_dwState & DS_NEEDSTOP)
			{
				pThis->StopDownload();
				pThis->setStateFlagsTo(0);
				pThis->Event(LS(L_DLDSTOPPED), EDT_RESPONSE_S);
				break;
			}

			if (pThis->m_dwState & DS_DOWNLOADING)
			{
				if (bAddSection)
				{
					fsTicksMgr curTicks;
					curTicks.Now();

					if (curTicks - pThis->m_ticksStart > 1200)
					{
						bAddSection = FALSE;
						pThis->AddSection();
					}
				}

				if (pThis->m_dldr.GetSpeed() == 0 && pThis->m_dldr.GetDownloadingSectionCount() != 0 &&
				    (pThis->m_dldr.IsResumeSupported() == RST_PRESENT || pThis->m_dldr.GetNumberOfSections() > 1))
				{
					fsTicksMgr tickNow;
					tickNow.Now();
					if (tickNow - tick0SpeedStart > 120 * 1000)
					{
						pThis->m_bNeedStartAgain = TRUE;
						pThis->StopDownload();
						tick0SpeedStart.Now();
						continue;
					}
				}
				else
					tick0SpeedStart.Now();
			}

			if (pThis->m_dwState & DS_NEEDADDSECTION)
			{
				pThis->removeStateFlags(DS_NEEDADDSECTION);
				pThis->AddSection();
			}

			if (pThis->m_dwState & DS_NEEDADDSECTION2)
			{
				pThis->removeStateFlags(DS_NEEDADDSECTION2);
				if (pThis->m_dldr.GetStoppedSectionCount())
					pThis->m_dldr.LaunchOneMoreSection();
				else
					pThis->AddSection(FALSE);
			}

			if (pThis->m_dwState & DS_NEEDRESTARTFROM)
			{
				pThis->removeStateFlags(DS_NEEDRESTARTFROM);

				pThis->setStateFlags(DS_NEEDSTART);
			}

			// pThis->CheckMirrSpeedRecalcRequired ();

			Sleep(100);
		}

	if (bSSR)
	{
		if (pThis->m_dp.dwFlags & DPF_RETRDATEFROMSERVER)
		{
			if (pThis->m_hOutFile == INVALID_HANDLE_VALUE)
			{
				pThis->m_hOutFile =
				    CreateFile(pThis->m_dp.strFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
			}

			FILETIME time;
			GetFileTime(pThis->m_hOutFile, &time, NULL, NULL);
			::SetFileTime(pThis->m_hOutFile, NULL, NULL, &time);
		}

		pThis->CloseFile();
	}

	pThis->m_bThreadRunning = FALSE;
	pThis->Event(DE_EXTERROR, DMEE_STOPPEDORDONE);

	InterlockedDecrement(&pThis->m_iThread);

	return 0;
}

DWORD fsDownloadMgr::_DownloaderEvents(fsDownloaderEvent enEvent, UINT uInfo, LPVOID lp)
{
	fsDownloadMgr* pThis = (fsDownloadMgr*)lp;
	fsTicksMgr curTicks;
	CHAR szEv[1000], szErr[1000];

	switch (enEvent)
	{
	case DE_SECTIONSTARTED:

		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, LS(L_STARTED));
		pThis->Event(szEv, EDT_RESPONSE_S);
		curTicks.Now();
		if (curTicks - pThis->m_ticksStart < 1200) break;
		break;

	case DE_SECTDOWNLOADING:

		pThis->m_bCantStart = FALSE;
		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, LS(L_DOWNLOADING));
		pThis->Event(szEv, EDT_RESPONSE_S);
		pThis->AddSection();
		break;

	case DE_MAYADDSECTION:
		pThis->AddSection();
		break;

	case DE_SPEEDISTOOLOW:
		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, LS(L_SPEEDISTOOLOW));
		pThis->Event(szEv, EDT_WARNING);
		break;

	case DE_SECTIONSTOPPED:

		pThis->m_bCantStart = FALSE;
		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, LS(L_SHESTOPPED));
		pThis->Event(szEv, EDT_RESPONSE_S);
		pThis->OnSectionStopped();
		break;

	case DE_SECTIONDONE:

		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, LS(L_DONE));
		pThis->Event(szEv, EDT_DONE);

		if (pThis->m_dldr.IsDone() && (pThis->m_dwState & DS_DONE) == 0 && pThis->m_dldr.IsRunning() == FALSE)
		{
			pThis->OnDone();
			pThis->Event(LS(L_DLDCOMPLETED), EDT_DONE);
			pThis->Event(enEvent, uInfo);
			pThis->setStateFlagsTo(DS_DONE);
			return 0;
		}
		else
		{
			if (pThis->m_dldr.IsAllSectionsOk())
				pThis->AddSection();
			else
				pThis->OnSectionStopped();
		}
		break;

	case DE_ERROROCCURED:
	{
		fsInternetResult ir = pThis->m_dldr.GetSectionLastError(uInfo);
		if (fsIRToStr(ir, szErr, sizeof(szErr)))
		{
			if (ir == IR_FILENOTFOUND)
				strcpy_s(szEv, sizeof(szEv), szErr);
			else
				sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, szErr);
			pThis->Event(szEv, EDT_RESPONSE_E);
		}

		if (pThis->m_dldr.GetDownloadingSectionCount() == 0) pThis->m_bCantStart = TRUE;
	}
	break;

	case DE_PAUSE:
	{
		char szPause[1000];
		sprintf(szPause, LS(L_PAUSESECS), pThis->m_dp.uRetriesTime / 1000);
		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, szPause);
		pThis->Event(szEv);
	}
	break;

	case DE_CONNECTING:
		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, LS(L_CONNECTING));
		pThis->Event(szEv);
		break;

	case DE_FAILCONNECT:
		if (fsIRToStr(pThis->m_dldr.GetSectionLastError(uInfo), szErr, sizeof(szErr)))
		{
			sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, szErr);
			pThis->Event(szEv, EDT_RESPONSE_E);
		}
		break;

	case DE_BADFILESIZE:
		pThis->Event(LS(L_FILESIZESARENOTEQUAL), EDT_RESPONSE_E);
		break;

	case DE_CONNECTED:
		sprintf(szEv, "[%s %d] - %s", LS(L_SECTION), uInfo + 1, LS(L_CONNSUCC));
		pThis->Event(szEv, EDT_RESPONSE_S);
		break;

	case DE_WRITEERROR:
		SetLastError(pThis->m_dldr.GetSectionLastError(uInfo));
		fsErrorToStr(szErr, sizeof(szErr));
		sprintf(szEv, "[%s %d] - %s - %s", LS(L_SECTION), uInfo + 1, LS(L_WRITEERR), szErr);
		pThis->Event(szEv, EDT_RESPONSE_E);
		pThis->Event(DE_EXTERROR, DMEE_FATALERROR);

		pThis->m_bFatalError = TRUE;
		break;

	case DE_REDIRECTING:
		pThis->Event(LS(L_REDIRECTING));
		break;

	case DE_REDIRECTINGOKCONTINUEOPENING:

		pThis->Event(LS(L_REDIRSUCC), EDT_RESPONSE_S);
		break;

	case DE_NEEDFILE:
		if (FALSE == pThis->Event(DE_NEEDFILE, uInfo)) return FALSE;
		return pThis->OnNeedFile();

	case DE_NEEDFILE_FINALINITIALIZATION:
		return pThis->OnNeedFile_FinalInit();

	case DE_SCR:
		return pThis->OnSCR();

	case DE_QUERYNEWSECTION:
		if (pThis->m_pfnEvents)
		{
			return pThis->Event(DE_QUERYNEWSECTION, uInfo);
		}
		break;

	case DE_ERRFROMSERVER:
	{
		pThis->m_strExtError = (LPCSTR)uInfo;
		LPCSTR pszErr1 = pThis->m_strExtError;
		CHAR _szErr[1000];
		fsIRToStr(IR_EXTERROR, _szErr, 1000);
		pThis->Event(_szErr, EDT_RESPONSE_E);
		pThis->Event(pszErr1, EDT_RESPONSE_E);
	}
	break;

	case DE_RESTARTINGBECAUSENORANGES:
		pThis->Event(LS(L_NORESUMERESTARTING), EDT_WARNING);
		break;

	case DE_DIALOGWITHSERVER:
		fsDlgWithServerInfo* info;
		info = (fsDlgWithServerInfo*)uInfo;
		pThis->Event(info->pszMsg, info->dir == IFDD_TOSERVER ? EDT_INQUIRY2 : EDT_RESPONSE_S2);
		break;

	case DE_FILESIZETOOBIG:
		pThis->Event(LS(L_FILESIZEEXCEEDS2GB), EDT_WARNING);
		break;

	case DE_CONFIRMARCHIVEDETECTION:
		return pThis->Event(DE_CONFIRMARCHIVEDETECTION, uInfo);

	case DE_ZIPPREVIEWSTARTED:
		pThis->Event("ZIP preview is in progress...");
		break;

	case DE_ZIPPREVIEWFAILED:
		pThis->Event("ZIP preview failed", EDT_RESPONSE_E);
		pThis->Event(DE_EXTERROR, DMEE_FATALERROR);
		pThis->m_bFatalError = TRUE;
		break;

	case DE_ARCHIVEDETECTED:
		pThis->Event("ZIP preview succeded", EDT_RESPONSE_S);
		return pThis->Event(DE_ARCHIVEDETECTED, uInfo);
	}

	pThis->Event(enEvent, uInfo);
	return TRUE;
}

fsInternetDownloader* fsDownloadMgr::GetDownloader()
{
	return &m_dldr;
}

fsInternetResult fsDownloadMgr::CreateByUrl(LPCSTR pszUrl, BOOL bAcceptHTMLPathes)
{
	// std::string auto-manages memory
	*m_dldr.DNP() = fsDownload_NetworkProperties();
	setDirty();

	CString strURL = pszUrl;
	strURL.Replace("&lt;", "<");
	strURL.Replace("&gt;", ">");
	strURL.Replace("&amp;", "&");
	strURL.Replace("&quot;", "\"");

	fsInternetResult ir = fsDNP_GetByUrl(m_dldr.DNP(), strURL);
	setDirty();
	if (ir != IR_SUCCESS) return ir;

	LPCSTR pszPathName = m_dldr.DNP()->strPathName.c_str();
	int len = lstrlen(pszPathName);

	if (pszPathName == NULL || len == 0 || pszPathName[len - 1] == '\\' || pszPathName[len - 1] == '/')
	{
		if (bAcceptHTMLPathes == FALSE || (m_dldr.DNP()->enProtocol != NP_HTTP && m_dldr.DNP()->enProtocol != NP_HTTPS))
			return IR_BADURL;
	}

	return IR_SUCCESS;
}

fsInternetResult fsDownloadMgr::StartDownload()
{
	fsInternetResult ir;

	Event(LS(L_STARTINGDLD));
	m_bFatalError = FALSE;

	ir = CreateInternetSession();
	if (ir != IR_SUCCESS) return ir;

	ApplyProperties();

	ir = m_dldr.StartDownloading(m_uNeedStartFrom);

	if (ir != IR_SUCCESS && ir != IR_S_FALSE && ir != IR_EXTERROR)
	{
		CHAR szEv[1000];
		BOOL bEv = FALSE;

		bEv = fsIRToStr(ir, szEv, sizeof(szEv));

		if (bEv) Event(szEv, EDT_RESPONSE_E);

		switch (ir)
		{
		case IR_FILENOTFOUND:

			if (m_dp.aEP[DFE_NOTFOUND] == DFEP_STOP) ir = IR_S_FALSE;
			break;

		case IR_LOGINFAILURE:
		case IR_INVALIDPASSWORD:
		case IR_INVALIDUSERNAME:

			if (m_dp.aEP[DFE_ACCDENIED] == DFEP_STOP) ir = IR_S_FALSE;
			break;
		}

		if (ir == IR_S_FALSE)
		{
			setStateFlags(DS_NEEDSTOP);
			Event(DE_EXTERROR, DMEE_FATALERROR);
			m_bFatalError = TRUE;
		}
	}

	if (ir == IR_S_FALSE && m_dldr.IsRunning() == FALSE && (m_dwState & DS_NEEDRESTARTFROM) == 0)
	{
		setStateFlags(DS_NEEDSTOP);
	}

	m_ticksStart.Now();

	return ir;
}

void fsDownloadMgr::StopDownloading()
{
	if (IsRunning())
	{
		setStateFlags(DS_NEEDSTOP);
		m_dldr.StopDownloading();
	}
}

void fsDownloadMgr::StopDownload()
{
	if (m_dldr.IsRunning())
	{
		m_dldr.StopDownloading();
		while (m_dldr.IsRunning()) Sleep(10);
	}
}

void fsDownloadMgr::SetOutputFileName(LPCSTR pszName)
{
	{
		size_t len = strlen(pszName) + 1;
		m_dp.strFileName = pszName;
	}
	setDirty();
}

void fsDownloadMgr::SetEventFunc(fntDownloadMgrEventFunc pfnEvents, LPVOID lpParam)
{
	m_pfnEvents = pfnEvents;
	m_lpParamEvents = lpParam;
}

void fsDownloadMgr::SetEventDescFunc(fntEventDescFunc pfn, LPVOID lpParam)
{
	m_pfnEventDesc = pfn;
	m_lpEventDescParam = lpParam;
}

void fsDownloadMgr::Event(LPCSTR pszEvent, fsDownloadMgr_EventDescType enType)
{
	if (m_pfnEventDesc && *pszEvent) m_pfnEventDesc(this, enType, pszEvent, m_lpEventDescParam);
}

void fsDownloadMgr::AddSection(BOOL bCheckAdm)
{
	if (m_bDontCreateNewSections || m_dldr.IsSectionCreatingNow()) return;

	if (bCheckAdm == FALSE || IsSectionCanBeAdded())
	{
		if (m_dldr.GetSectionMaxSize() > m_dp.uSectionMinSize)
		{
			Event(LS(L_NEWSECTION));
			m_lastError = m_dldr.AddSection(bCheckAdm);

			if (m_lastError != IR_SUCCESS)
			{
				if (m_lastError == IR_S_FALSE)
					Event(LS(L_CANCELED), EDT_RESPONSE_S);
				else
				{
					CHAR szEv[1000];
					if (fsIRToStr(m_lastError, szEv, sizeof(szEv))) Event(szEv, EDT_RESPONSE_E);
				}
			}
		}
	}
}

BOOL fsDownloadMgr::IsDone()
{
	return m_dldr.IsDone();
}

BOOL fsDownloadMgr::LoadState(LPVOID lpBuffer, LPDWORD pdwSize, WORD wVer)
{
	// Old serialization format is no longer supported after LPSTR->std::string migration
	return FALSE;
}

BOOL fsDownloadMgr::IsRunning()
{
	return m_bThreadRunning || m_dldr.IsRunning();
}

BOOL fsDownloadMgr::SleepInterval()
{
	int i = m_dp.uRetriesTime;

	while (i > 0)
	{
		Sleep(100);
		i -= 100;

		if (m_dwState & DS_NEEDSTOP) return FALSE;
	}

	return TRUE;
}

void fsDownloadMgr::OnSectionStopped()
{
	if (m_dldr.IsRunning() == FALSE)
	{
		removeStateFlags(DS_DOWNLOADING);

		if ((m_bNeedStartAgain || m_dldr.IsStoppedByUser() == FALSE) && m_bFatalError == FALSE)
		{
			m_bNeedStartAgain = FALSE;
			Event(LS(L_RESTARTINGDLD));
			setStateFlags(DS_NEEDSTART);
		}
		else
		{
			setStateFlags(DS_NEEDSTOP);
		}
	}
}

BOOL fsDownloadMgr::OnNeedFile()
{
	BOOL bOk;

	if (m_hOutFile != INVALID_HANDLE_VALUE)
	{

		m_dldr.SetOutputFile(m_hOutFile);
		return TRUE;
	}

	Event(LS(L_OPENINGFILE));

	if (m_dwDownloadFileFlags & DFF_NEED_INIT_FILE)
	{
		if (FALSE == InitFile())
		{
			Event(DE_EXTERROR, DMEE_FATALERROR);
			m_bFatalError = TRUE;
			return FALSE;
		}
	}

	bOk = OpenFile();

	if (!bOk)
		goto _lErr;
	else if (bOk == BOOL(-1))
		return FALSE;

	m_dldr.SetOutputFile(m_hOutFile);
	Event(LS(L_SUCCESS), EDT_RESPONSE_S);

	return TRUE;

_lErr:
	DescribeAPIError();
	setStateFlags(DS_NEEDSTOP);
	Event(DE_EXTERROR, DMEE_FATALERROR);
	m_bFatalError = TRUE;
	return FALSE;
}

void fsDownloadMgr::RenameFile(BOOL bFormat1)
{
	int i = 1;
	DWORD dwResult;
	CHAR szFileWE[MY_MAX_PATH];
	CString strFile;

	strcpy_s(szFileWE, sizeof(szFileWE), m_dp.strFileName.c_str());

	if (!m_dp.strAdditionalExt.empty())
	{
		int fl = strlen(szFileWE);
		int al = m_dp.strAdditionalExt.length();

		if (fl > al && szFileWE[fl - al - 1] == '.' && stricmp(szFileWE + fl - al, m_dp.strAdditionalExt.c_str()) == 0)
		{
			szFileWE[fl - al - 1] = 0;
		}
	}

	LPSTR pszExt = strrchr(szFileWE, '.');
	LPSTR pszDirEnd = strrchr(szFileWE, '\\');

	if (pszExt != NULL && pszDirEnd > pszExt) pszExt = NULL;

	if (pszExt) *pszExt = 0;

	if (m_bRename_CheckIfRenamed)
	{
		int l = lstrlen(szFileWE);
		if (szFileWE[l - 1] == ')')
		{
			LPSTR psz = szFileWE + l - 2;
			while (*psz && *psz >= '0' && *psz <= '9') psz--;
			if (*psz == '(') *psz = 0;
		}
	}

	m_csRenameFile.Lock();

	do
	{
		if (pszExt)
			strFile.Format("%s(%d).%s", szFileWE, i++, pszExt + 1);
		else
			strFile.Format("%s(%d)", szFileWE, i++);

		dwResult = GetFileAttributes(strFile);
	} while (dwResult != DWORD(-1));
	{
		size_t len = strFile.GetLength() + 1;
		m_dp.strFileName = strFile;
	}
	setDirty();

	HANDLE hFile = CreateFile(m_dp.strFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

	m_csRenameFile.Unlock();

	CHAR szFileName[MY_MAX_PATH];
	fsGetFileName(strFile, szFileName);
	CString strEv;
	if (bFormat1)
		strEv.Format("%s \"%s\"", LS(L_FILEALREXISTSRENAMING), szFileName);
	else
		strEv.Format("%s %s", LS(L_RENAMINGTO), szFileName);

	Event(strEv, EDT_WARNING);
	Event(DE_EXTERROR, DMEE_FILEUPDATED);
}

void fsDownloadMgr::RenameFile(const char* szFileName, BOOL bFormat1)
{
	int i = 1;
	DWORD dwResult;
	CHAR szFileWE[MY_MAX_PATH];
	CString strFile;

	strcpy_s(szFileWE, sizeof(szFileWE), szFileName);

	if (!m_dp.strAdditionalExt.empty())
	{
		int fl = strlen(szFileWE);
		int al = m_dp.strAdditionalExt.length();

		if (fl > al && szFileWE[fl - al - 1] == '.' && stricmp(szFileWE + fl - al, m_dp.strAdditionalExt.c_str()) == 0)
		{
			szFileWE[fl - al - 1] = 0;
		}
	}

	LPSTR pszExt = strrchr(szFileWE, '.');
	LPSTR pszDirEnd = strrchr(szFileWE, '\\');

	if (pszExt != NULL && pszDirEnd > pszExt) pszExt = NULL;

	if (pszExt) *pszExt = 0;

	if (m_bRename_CheckIfRenamed)
	{
		int l = lstrlen(szFileWE);
		if (szFileWE[l - 1] == ')')
		{
			LPSTR psz = szFileWE + l - 2;
			while (*psz && *psz >= '0' && *psz <= '9') psz--;
			if (*psz == '(') *psz = 0;
		}
	}

	m_csRenameFile.Lock();

	int nIndex = 0;
	do
	{
		if (pszExt)
			strFile.Format("%s(%d).%s", szFileWE, i++, pszExt + 1);
		else
			strFile.Format("%s(%d)", szFileWE, i++);

		nIndex = i;
		dwResult = GetFileAttributes(strFile);
	} while (dwResult != DWORD(-1));

	int nExtIndex = 0;
	i = 1;
	do
	{
		if (pszExt)
			strFile.Format("%s(%d).%s.%s", szFileWE, i++, pszExt + 1, m_dp.strAdditionalExt.c_str());
		else
			strFile.Format("%s(%d).%s", szFileWE, i++, m_dp.strAdditionalExt.c_str());

		nExtIndex = i;
		dwResult = GetFileAttributes(strFile);
	} while (dwResult != DWORD(-1));

	nIndex = max(nIndex, nExtIndex) - 1;
	if (pszExt)
		strFile.Format("%s(%d).%s.%s", szFileWE, nIndex, pszExt + 1, m_dp.strAdditionalExt.c_str());
	else
		strFile.Format("%s(%d).%s", szFileWE, nIndex, m_dp.strAdditionalExt.c_str());
	{
		size_t len = strFile.GetLength() + 1;
		m_dp.strFileName = strFile;
	}
	setDirty();

	HANDLE hFile = CreateFile(m_dp.strFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

	m_csRenameFile.Unlock();

	CHAR szFileName_[MY_MAX_PATH];
	fsGetFileName(strFile, szFileName_);
	CString strEv;
	if (bFormat1)
		strEv.Format("%s \"%s\"", LS(L_FILEALREXISTSRENAMING), szFileName_);
	else
		strEv.Format("%s %s", LS(L_RENAMINGTO), szFileName_);

	Event(strEv, EDT_WARNING);
	Event(DE_EXTERROR, DMEE_FILEUPDATED);
}

BOOL fsDownloadMgr::OpenFile(BOOL bFailIfDeleted, BOOL bDisableEvents)
{
	if (m_hOutFile != INVALID_HANDLE_VALUE) return TRUE;

	if (bFailIfDeleted && GetFileAttributes(m_dp.strFileName.c_str()) == DWORD(-1) && m_dldr.GetNumberOfSections())
	{
		fsSection sect;
		m_dldr.GetSectionInfo(0, &sect);

		if (m_dldr.GetNumberOfSections() != 1 || sect.uCurrent != sect.uStart)
		{
			if (bDisableEvents == FALSE) Event(LS(L_WASDELETED), EDT_RESPONSE_E);
			setStateFlags(DS_NEEDSTOP);

			if (bDisableEvents == FALSE) Event(DE_EXTERROR, DMEE_FILEWASDELETED);
			return -1;
		}
	}

	DWORD dwFileAttribs = FILE_ATTRIBUTE_NORMAL;

	m_hOutFile = CreateFile(m_dp.strFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, dwFileAttribs, NULL);

	DWORD dw = GetFileAttributes(m_dp.strFileName.c_str());
	if ((m_dp.dwFlags & DPF_USEHIDDENATTRIB) && (dw & FILE_ATTRIBUTE_HIDDEN) == 0)
		SetFileAttributes(m_dp.strFileName.c_str(), dw | FILE_ATTRIBUTE_HIDDEN);

	if (m_hOutFile == INVALID_HANDLE_VALUE)
	{
		m_bFailedToCreateDestinationFile = true;
		return FALSE;
	}

	if (::GetLastError() != ERROR_ALREADY_EXISTS) SetFileTime(m_hOutFile);

	return TRUE;
}

BOOL fsDownloadMgr::TruncFile(const CString& sFileName)
{
	DWORD dwFileAttribs = FILE_ATTRIBUTE_NORMAL;

	HANDLE hFile =
	    CreateFile((LPCTSTR)sFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, dwFileAttribs, NULL);

	DWORD dw = GetFileAttributes((LPCTSTR)sFileName);
	if ((m_dp.dwFlags & DPF_USEHIDDENATTRIB) && (dw & FILE_ATTRIBUTE_HIDDEN) == 0)
		SetFileAttributes((LPCTSTR)sFileName, dw | FILE_ATTRIBUTE_HIDDEN);

	if (hFile == INVALID_HANDLE_VALUE) return FALSE;

	if (::GetLastError() != ERROR_ALREADY_EXISTS) SetFileTime(m_hOutFile);

	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	SetEndOfFile(hFile);

	::CloseHandle(hFile);

	return TRUE;
}

void fsDownloadMgr::RemoveIncompleteFileExt()
{
	int fl = m_dp.strFileName.length();
	int el = m_dp.strAdditionalExt.length();
	if (fl > el && m_dp.strFileName[fl - el - 1] == '.' &&
	    stricmp(m_dp.strFileName.c_str() + fl - el, m_dp.strAdditionalExt.c_str()) == 0)
	{
		m_dp.strFileName[fl - el - 1] = 0;
		setDirty();
	}
}

BOOL fsDownloadMgr::ApplyAER(fsAlreadyExistReaction enAER, bool bFirstCheck)
{
	switch (enAER)
	{
	case AER_ASKUSER:
	{
		CAERDlg dlg;

		if (bFirstCheck)
			dlg.m_pszFile = m_dp.strFileName.c_str();
		else
			dlg.m_pszFile = (LPCTSTR)m_sOriginalFile;

		_DlgMgr.OnDoModal(&dlg);
		dlg.DoModal();
		_DlgMgr.OnEndDialog(&dlg);
		if (dlg.m_bDontAskAgain)
		{
			m_dp.enAER = dlg.m_enAER;
			setDirty();
			_App.AlreadyExistReaction(dlg.m_enAER);
			_DldsMgr.ApplyAER(dlg.m_enAER);
		}
		enAER = dlg.m_enAER;

		return ApplyAER(enAER, bFirstCheck);
	}

	case AER_REWRITE:
		if (!bFirstCheck)
		{
			if (m_sOriginalFile.CompareNoCase(m_dp.strFileName.c_str()) != 0) RemoveIncompleteFileExt();
		}

		if (!OpenFile()) return FALSE;
		Event(LS(L_REWRITINGIT), EDT_WARNING);
		SetFilePointer(m_hOutFile, 0, NULL, FILE_BEGIN);
		SetEndOfFile(m_hOutFile);
		return TRUE;

	case AER_RENAME_2:
	case AER_RENAME:
		if (!bFirstCheck)
		{
			RenameFile((LPCTSTR)m_sOriginalFile);
			return TRUE;
		}

		RenameFile();
		return TRUE;

	case AER_RESUME:
		if (!bFirstCheck)
		{
			if (m_sOriginalFile.CompareNoCase(m_dp.strFileName.c_str()) != 0) RemoveIncompleteFileExt();
		}

		if (!OpenFile()) return FALSE;
		m_uNeedStartFrom = GetFileSize(m_hOutFile, NULL);
		Event(LS(L_RESUMINGDLD), EDT_WARNING);
		setStateFlags(DS_NEEDRESTARTFROM);
		return -1;

	case AER_STOP:
		if (m_sOriginalFile.CompareNoCase(m_dp.strFileName.c_str()) != 0) RemoveIncompleteFileExt();

		Event(LS(L_ALREXISTS), EDT_RESPONSE_E);
		setStateFlags(DS_NEEDSTOP);
		Event(DE_EXTERROR, DMEE_USERSTOP);
		return -1;

	default:
		ASSERT(FALSE);
		return FALSE;
	}
}

DWORD fsDownloadMgr::Event(fsDownloaderEvent ev, UINT uInfo)
{
	if (m_pfnEvents) return m_pfnEvents(this, ev, uInfo, m_lpParamEvents);

	return TRUE;
}

BOOL fsDownloadMgr::BuildFileName(LPCSTR pszSetExt)
{
	CHAR szFile[MY_MAX_PATH] = "";
	CHAR szPath[MY_MAX_PATH] = "";

	int fl = m_dp.strFileName.length();

	if (fl > 1 && m_dp.strFileName[fl - 1] != '/' && m_dp.strFileName[fl - 1] != '\\') return TRUE;

	LPCSTR pszSuggFile = m_dldr.GetSuggestedFileName();
	if (pszSuggFile && *pszSuggFile)
	{
		strcpy_s(szFile, sizeof(szFile), pszSuggFile);
	}
	else
	{
		if (!fsFileNameFromUrlPath(GetDNP()->strPathName.c_str(), GetDNP()->enProtocol == NP_FTP, TRUE, szFile, sizeof(szFile)))
			return FALSE;
	}

	if (*szFile)
	{
		_COM_SMARTPTR_TYPEDEF(IMultiLanguage2, __uuidof(IMultiLanguage2));
		IMultiLanguage2Ptr spML;
		HRESULT hrCoInit = CoInitialize(NULL);
		spML.CreateInstance(CLSID_CMultiLanguage);
		if (spML != NULL)
		{
			DetectEncodingInfo enc = {0};
			int iEncLen = 1;
			size_t bufSize = max(strlen(szFile), 1024) + 1;
			char* pszBuf = new char[bufSize];
			strcpy_s(pszBuf, bufSize, szFile);
			while (strlen(pszBuf) < 300) strcat_s(pszBuf, bufSize, szFile);
			int iLen = strlen(pszBuf);
			if (S_OK == spML->DetectInputCodepage(MLDETECTCP_8BIT, 0, pszBuf, &iLen, &enc, &iEncLen) && iEncLen == 1)
			{
				if (enc.nCodePage == CP_UTF8)
				{
					iLen = strlen(szFile);
					LPWSTR pwsz = new wchar_t[iLen + 1];
					*pwsz = 0;
					iLen = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)szFile, iLen, pwsz, iLen);
					if (iLen > 0) pwsz[iLen] = 0;
					WideCharToMultiByte(CP_ACP, 0, pwsz, -1, szFile, sizeof(szFile), NULL, NULL);
					delete[] pwsz;
				}
			}
			delete[] pszBuf;
			spML = NULL;
		}
		if (SUCCEEDED(hrCoInit)) CoUninitialize();
	}

	if (*szFile == 0) strcpy_s(szFile, sizeof(szFile), "index.html");

	LPSTR psz = szFile;
	char szSymbls[] = {":*?\"<>|"};
	while (*psz)
	{
		if (strchr(szSymbls, *psz)) *psz = '_';
		psz++;
	}

	if (fl >= MY_MAX_PATH - 1) return FALSE;

	szFile[MY_MAX_PATH - 1 - fl] = 0;

	char* pszExt = strrchr(szFile, '.');

	if (pszSetExt)
	{
		if (pszExt == NULL)
		{
			strcat_s(szFile, sizeof(szFile), ".");
			strcat_s(szFile, sizeof(szFile), pszSetExt);
		}
		else
		{
			strcpy_s(pszExt + 1, sizeof(szFile) - (pszExt + 1 - szFile), pszSetExt);
		}
	}
	else if (pszExt == NULL && !m_dp.strCreateExt.empty())
	{

		strcat_s(szFile, sizeof(szFile), ".");
		strcat_s(szFile, sizeof(szFile), m_dp.strCreateExt.c_str());
	}

	strcpy_s(szPath, sizeof(szPath), m_dp.strFileName.c_str());
	strcat_s(szPath, sizeof(szPath), szFile);
	{
		size_t len = strlen(szPath) + 1;
		m_dp.strFileName = szPath;
	}
	setDirty();

	Event(DE_EXTERROR, DMEE_FILEUPDATED);

	return TRUE;
}

BOOL fsDownloadMgr::ReserveDiskSpace()
{
	if (FALSE == m_dp.bReserveDiskSpace || m_dldr.GetLDFileSize() == _UI64_MAX) return TRUE;

	ULARGE_INTEGER liSize = {0};
	liSize.LowPart = GetFileSize(m_hOutFile, &liSize.HighPart);
	if (liSize.QuadPart == 0)
	{
		m_dldr.LockWriteFile(TRUE);
		bool bOK = fsSetFilePointer(m_hOutFile, m_dldr.GetLDFileSize(), FILE_BEGIN) && SetEndOfFile(m_hOutFile);
		m_dldr.LockWriteFile(FALSE);
		if (!bOK)
		{
			m_bIsNotEnoughDiskSpace = true;
			return FALSE;
		}
	}

	if (m_dldr.GetNumberOfSections() == 1 && m_dldr.GetLDFileSize() > 100 * 1024 * 1024)
	{
		m_bDontCreateNewSections = TRUE;
		InterlockedIncrement(&m_iThread);
		DWORD dw;
		CloseHandle(CreateThread(NULL, 0, _threadReserveDiskSpace, this, 0, &dw));
	}

	return TRUE;
}

void fsDownloadMgr::ApplyAdditionalExt()
{
	CHAR szFile[MY_MAX_PATH];
	int fl = m_dp.strFileName.length();
	int el = m_dp.strAdditionalExt.length();

	if (el == 0) return;

	if (fl > el)
	{
		if (stricmp(m_dp.strFileName.c_str() + fl - el, m_dp.strAdditionalExt.c_str()) == 0 && m_dp.strFileName[fl - el - 1] == '.')
			return;

		if (fl + el >= MY_MAX_PATH) return;
	}

	strcpy_s(szFile, sizeof(szFile), m_dp.strFileName.c_str());
	strcat_s(szFile, sizeof(szFile), ".");
	strcat_s(szFile, sizeof(szFile), m_dp.strAdditionalExt.c_str());
	{
		size_t len = strlen(szFile) + 1;
		m_dp.strFileName = szFile;
	}
	setDirty();
}

DWORD fsDownloadMgr::OnSCR()
{
	return ProcessSCR(m_dp.enSCR, TRUE);
}

DWORD fsDownloadMgr::ProcessSCR(fsSizeChangeReaction scr, BOOL bFirstCall)
{
	if (bFirstCall)
	{
		if (FALSE == OpenFile())
		{
			DWORD dwLastError = GetLastError();
			Event(LS(L_FAILEDTOOPEN), EDT_RESPONSE_E);
			DescribeAPIError(&dwLastError);
			setStateFlags(DS_NEEDSTOP);
			return SCR_STOP;
		}
		Event(LS(L_FILESIZEWASCHANGED), EDT_WARNING);
	}

	switch (scr)
	{
	case SCR_ASKUSER:
	{
		CSCRDlg dlg;
		dlg.m_dnp = GetDNP();
		_DlgMgr.OnDoModal(&dlg);
		dlg.DoModal();
		_DlgMgr.OnEndDialog(&dlg);
		if (dlg.m_bDontAskAgain)
		{
			m_dp.enSCR = dlg.m_enSCR;
			setDirty();
			_App.SizeChangeReaction(dlg.m_enSCR);
		}

		return ProcessSCR(dlg.m_enSCR, FALSE);
	}

	case SCR_RESTART:
		Event(LS(L_RESTARTINGDLD), EDT_WARNING);
		SetFilePointer(m_hOutFile, 0, NULL, FILE_BEGIN);
		SetEndOfFile(m_hOutFile);
		break;

	case SCR_ADJUSTFORNEWSIZE:
	{
		Event(LS(L_ADJFORNEWSIZE), EDT_WARNING);

		UINT64 uNewSize = m_dldr.GetSSFileSize();
		UINT64 uOldSize = GetFileSize(m_hOutFile, NULL);

		if (uOldSize > uNewSize)
		{
			fsSetFilePointer(m_hOutFile, uNewSize, FILE_BEGIN);
			SetEndOfFile(m_hOutFile);
		}
	}
	break;

	case SCR_STOP:
		setStateFlags(DS_NEEDSTOP);
		Event(DE_EXTERROR, DMEE_USERSTOP);
		break;

	default:
		ASSERT(5 != 5);
	}

	return scr;
}

void fsDownloadMgr::DescribeAPIError(DWORD* pdwLastError)
{
	CHAR szErr[1000];
	fsErrorToStr(szErr, sizeof(szErr), pdwLastError);
	Event(szErr, EDT_RESPONSE_E);
}

void fsDownloadMgr::OnDone()
{
	CloseFile();
	RemoveHiddenAttribute();

	int fl = m_dp.strFileName.length();
	int el = m_dp.strAdditionalExt.length();

	if (el == 0 || el >= fl - 1)
	{
		if (m_dp.dwFlags & DPF_APPENDCOMMENTTOFILENAME) AppendCommentToFileName(TRUE);
		return;
	}

	if (fsStrNCmpNC(m_dp.strFileName.c_str() + fl - el, m_dp.strAdditionalExt.c_str(), el))
	{
		if (m_dp.dwFlags & DPF_APPENDCOMMENTTOFILENAME) AppendCommentToFileName(TRUE);
		return;
	}

	CHAR szFileNameFrom[MY_MAX_PATH];
	strcpy_s(szFileNameFrom, sizeof(szFileNameFrom), m_dp.strFileName.c_str());

	m_dp.strFileName[fl - el - 1] = 0;
	setDirty();

	if (m_dp.dwFlags & DPF_APPENDCOMMENTTOFILENAME) AppendCommentToFileName(FALSE);

	CheckDstFileExists();

	if (DWORD(-1) != GetFileAttributes(m_dp.strFileName.c_str())) ::DeleteFile(m_dp.strFileName.c_str());
	if (FALSE == ::MoveFile(szFileNameFrom, m_dp.strFileName.c_str()))
	{
		DWORD dwLastError = GetLastError();
		Event(LS(L_CANTRENAMEBACK), EDT_RESPONSE_E);
		DescribeAPIError(&dwLastError);
		m_dp.strFileName = szFileNameFrom;
		setDirty();
	}
}

BOOL fsDownloadMgr::DeleteFile()
{
	if (m_dwDownloadFileFlags & DFF_NEED_INIT_FILE) return TRUE;

	StopDownloading();

	CloseFile();

	if (GetFileAttributes(m_dp.strFileName.c_str()) != DWORD(-1))
	{
		fsString str = m_dp.strFileName.c_str();
		str += ".dsc.txt";
		::DeleteFile(str);
		return ::DeleteFile(m_dp.strFileName.c_str());
	}
	else
		return TRUE;
}

void fsDownloadMgr::CloseFile()
{
	if (m_hOutFile != INVALID_HANDLE_VALUE)
	{
		m_dldr.SetOutputFile(INVALID_HANDLE_VALUE);
		CloseHandle(m_hOutFile);
		m_hOutFile = INVALID_HANDLE_VALUE;
	}
}

void fsDownloadMgr::CreateCompleteDownload(UINT64 uFileSize)
{
	m_dldr.SetFileSize(uFileSize);
	m_dldr.CreateCompleteSection(uFileSize);
	setStateFlagsTo(DS_DONE);
}

void fsDownloadMgr::AddSection(UINT64 uStart, UINT64 uEnd, UINT64 uCurrent)
{
	m_dldr.AddSection(uStart, uEnd, uCurrent);
}

void fsDownloadMgr::SetFileSize(UINT64 uFileSize)
{
	m_dldr.SetFileSize(uFileSize);
}

DWORD fsDownloadMgr::GetDownloadFileFlag() const
{
	return m_dwDownloadFileFlags;
}

void fsDownloadMgr::SetDownloadFileFlag(DWORD dwFlag)
{
	m_dwDownloadFileFlags = dwFlag;
}

fsInternetResult fsDownloadMgr::GetLastError()
{
	return m_lastError;
}

BOOL fsDownloadMgr::InitFile(BOOL bCreateOnDisk, LPCSTR pszSetExt)
{
	CString strFileName;

	if (m_dp.strFileName.length() > 0)
	{
		InitFile_ProcessMacroses();
	}

	if (FALSE == BuildFileName(pszSetExt))
	{
		Event(LS(L_FILENAMETOOLONG), EDT_RESPONSE_E);
		setStateFlags(DS_NEEDSTOP);
		m_bFatalError = TRUE;
		return FALSE;
	}

	bool bIsIncFileExt = ((!m_dp.strAdditionalExt.empty()) && (!m_dp.strAdditionalExt.empty()));
	m_sOriginalFile = m_dp.strFileName.c_str();

	ApplyAdditionalExt();

	if (!fsBuildPathToFile(m_dp.strFileName.c_str())) goto _lErr;

	if (DWORD(-1) != GetFileAttributes(m_dp.strFileName.c_str()))
	{
		fsAlreadyExistReaction enAER = m_dp.enAER;

		BOOL bRet = ApplyAER(enAER);

		if (bRet == FALSE)
			goto _lErr;
		else if (bRet == BOOL(-1))
			return FALSE;
	}
	else
	{
		if (bIsIncFileExt && DWORD(-1) != GetFileAttributes(m_sOriginalFile))
		{
			fsAlreadyExistReaction enAER = m_dp.enAER;

			BOOL bRet = ApplyAER(enAER, false);

			if (bRet == FALSE)
				goto _lErr;
			else if (bRet == BOOL(-1))
				return FALSE;
		}
	}

	if (bCreateOnDisk)
	{
		HANDLE hFile =
		    CreateFile(m_dp.strFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile == INVALID_HANDLE_VALUE) goto _lErr;

		SetFileTime(hFile);

		CloseHandle(hFile);
	}

	m_dwDownloadFileFlags &= ~DFF_NEED_INIT_FILE;
	setDirty();

	return TRUE;

_lErr:
	DescribeAPIError();
	m_bFatalError = TRUE;
	setStateFlags(DS_NEEDSTOP);
	return FALSE;
}

BOOL fsDownloadMgr::IsFileInit()
{
	return (m_dwDownloadFileFlags & DFF_NEED_INIT_FILE) == 0;
}

BOOL fsDownloadMgr::IsSectionCanBeAdded()
{
	if (m_dldr.IsHavingError()) return FALSE;

	if ((m_dwState & DS_NEEDSTOP) || ((m_dwState & DS_DOWNLOADING) == 0)) return FALSE;

	if (m_dldr.IsAllSectionsOk() == FALSE) return FALSE;

	if (UINT(m_dldr.GetNumberOfSections() - m_dldr.GetDoneSectionCount()) < m_dp.uMaxSections) return TRUE;

	return FALSE;
}

fsInternetResult fsDownloadMgr::RestartDownloading()
{
	fsInternetResult ir = SetToRestartState();

	std::string dir = m_dp.strFileName;
	int dirEnd = dir.rfind("\\");
	if (dirEnd != -1 && (size_t)dirEnd < dir.length() - 1)
	{
		m_dp.strFileName[dirEnd + 1] = 0;
	}

	if (ir != IR_SUCCESS) return ir;

	return StartDownloading();
}

void fsDownloadMgr::StopSection()
{
	if (IsRunning() == FALSE) return;

	if (m_dldr.GetRunningSectionCount() == 1)
		StopDownloading();
	else
		m_dldr.StopSection();
}

void fsDownloadMgr::CreateOneMoreSection()
{
	if (m_dldr.IsSectionCreatingNow() == FALSE)
	{
		setStateFlags(DS_NEEDADDSECTION2);
		setDirty();
	}
}

fsInternetResult fsDownloadMgr::QuerySize(BOOL bCheckPoss)
{
	if (bCheckPoss)
		if (IsRunning() || IsQueringSize()) return IR_S_FALSE;

	fsInternetResult ir;

	setStateFlags(DS_QUERINGSIZE);

	ir = CreateInternetSession();
	if (ir != IR_SUCCESS)
	{
		removeStateFlags(DS_QUERINGSIZE);
		return ir;
	}

	ApplyProperties();

	ir = m_dldr.QuerySize();

	removeStateFlags(DS_QUERINGSIZE);
	return ir;
}

BOOL fsDownloadMgr::IsQueringSize()
{
	return m_dwState & DS_QUERINGSIZE;
}

void fsDownloadMgr::QuerySize2()
{
	if (IsRunning() || IsQueringSize()) return;

	setStateFlags(DS_QUERINGSIZE);

	DWORD dw;
	InterlockedIncrement(&m_iThread);
	CloseHandle(CreateThread(NULL, 0, _threadQSize, this, 0, &dw));
}

DWORD WINAPI fsDownloadMgr::_threadQSize(LPVOID lp)
{
	fsDownloadMgr* pThis = (fsDownloadMgr*)lp;
	try
	{
		pThis->Event(LS(L_QUERINGSIZE));
		fsInternetResult ir = pThis->QuerySize(FALSE);
		if (ir != IR_SUCCESS)
		{
			char szErr[10000];
			fsIRToStr(ir, szErr, sizeof(szErr));
			pThis->Event(szErr, EDT_RESPONSE_E);
		}
		else
			pThis->Event(LS(L_DONE), EDT_DONE);
		pThis->Event(DE_EXTERROR, DMEE_FILEUPDATED);
	}
	catch (const std::exception& ex)
	{
		ASSERT(FALSE);
		vmsLogger::WriteLog("fsDownloadMgr::_threadQSize " + tstring(ex.what()));
	}
	catch (...)
	{
		ASSERT(FALSE);
		vmsLogger::WriteLog("fsDownloadMgr::_threadQSize unknown exception");
	}

	InterlockedDecrement(&pThis->m_iThread);
	return 0;
}

void fsDownloadMgr::StopQuering()
{
	if (IsQueringSize())
	{
		m_dldr.StopDownloading();
	}
}

BOOL fsDownloadMgr::IsCantStart()
{
	return m_bCantStart;
}

void fsDownloadMgr::CloneSettings(fsDownloadMgr* src)
{
	fsDownload_Properties* dp = src->GetDP();
	fsDownload_NetworkProperties* dnp = src->GetDNP();
	fsDownload_NetworkProperties* mydnp = GetDNP();

	CopyMemory(m_dp.aEP, dp->aEP, sizeof(m_dp.aEP));
	m_dp.dwFlags = dp->dwFlags;
	m_dp.bIgnoreRestrictions = dp->bIgnoreRestrictions;
	m_dp.bReserveDiskSpace = dp->bReserveDiskSpace;
	m_dp.bRestartSpeedLow = dp->bRestartSpeedLow;
	m_dp.enAER = dp->enAER;
	m_dp.enSCR = dp->enSCR;
	{
		size_t len = dp->strAdditionalExt.length() + 1;
		m_dp.strAdditionalExt = dp->strAdditionalExt;
	}
	{
		size_t len = dp->strCreateExt.length() + 1;
		m_dp.strCreateExt = dp->strCreateExt;
	}

	if (m_dp.strFileName.empty() || m_dp.strFileName.empty() || m_dp.strFileName[m_dp.strFileName.length() - 1] == '\\' ||
	    m_dp.strFileName[m_dp.strFileName.length() - 1] == '/')
	{
		if (!dp->strFileName.empty())
		{
			{
				size_t len = dp->strFileName.length() + 1;
				m_dp.strFileName = dp->strFileName;
			}
		}
	}
	m_dp.uMaxAttempts = dp->uMaxAttempts;
	m_dp.uMaxSections = dp->uMaxSections;
	m_dp.uRetriesTime = dp->uRetriesTime;
	m_dp.uSectionMinSize = dp->uSectionMinSize;
	m_dp.uTimeout = dp->uTimeout;
	m_dp.uTrafficRestriction = dp->uTrafficRestriction;

	mydnp->dwFtpFlags = dnp->dwFtpFlags;
	mydnp->bUseCookie = dnp->bUseCookie;
	mydnp->bUseHttp11 = dnp->bUseHttp11;
	mydnp->enAccType = dnp->enAccType;
	mydnp->enFtpTransferType = dnp->enFtpTransferType;
	{
		size_t len = dnp->strAgent.length() + 1;
		mydnp->strAgent = dnp->strAgent;
	}
	{
		size_t len = dnp->strASCIIExts.length() + 1;
		mydnp->strASCIIExts = dnp->strASCIIExts;
	}
	{
		size_t len = dnp->strReferer.length() + 1;
		mydnp->strReferer = dnp->strReferer;
	}
	{
		size_t len = dnp->strUserName.length() + 1;
		mydnp->strUserName = dnp->strUserName;
	}
	{
		size_t len = dnp->strPassword.length() + 1;
		mydnp->strPassword = dnp->strPassword;
	}
}

fsInternetResult fsDownloadMgr::SetToRestartState()
{
	if (IsRunning()) return IR_S_FALSE;

	if (IsFileInit())
	{
		if (m_dp.enAER != AER_RENAME_2)
			DeleteFile();
		else
			m_bRename_CheckIfRenamed = TRUE;
		m_dwDownloadFileFlags |= DFF_NEED_INIT_FILE;
		setDirty();
	}

	m_dldr.DeleteAllSections();

	return IR_SUCCESS;
}

BOOL fsDownloadMgr::OnNeedFile_FinalInit()
{
	SetFileTime(m_hOutFile);

	if (!ReserveDiskSpace())
	{
		if (m_dwState & DS_NEEDSTOP) return FALSE;
		goto _lErr;
	}

	return TRUE;

_lErr:
	DescribeAPIError();
	setStateFlags(DS_NEEDSTOP);
	Event(DE_EXTERROR, DMEE_FATALERROR);
	m_bFatalError = TRUE;
	return FALSE;
}

void fsDownloadMgr::RemoveHiddenAttribute()
{
	if (m_dp.dwFlags & DPF_USEHIDDENATTRIB)
	{
		DWORD dw = GetFileAttributes(m_dp.strFileName.c_str());
		dw &= ~FILE_ATTRIBUTE_HIDDEN;
		SetFileAttributes(m_dp.strFileName.c_str(), dw);
	}
}

void fsDownloadMgr::CheckDstFileExists()
{
	if (GetFileAttributes(m_dp.strFileName.c_str()) != DWORD(-1))
	{
		fsAlreadyExistReaction enAER = m_dp.enAER;

		if (enAER == AER_ASKUSER)
		{
			CAERDlg dlg;

			dlg.m_pszFile = m_dp.strFileName.c_str();
			dlg.DisableStopAndResume();
			_DlgMgr.OnDoModal(&dlg);
			dlg.DoModal();
			_DlgMgr.OnEndDialog(&dlg);

			if (dlg.m_bDontAskAgain)
			{
				m_dp.enAER = dlg.m_enAER;
				setDirty();
				_App.AlreadyExistReaction(dlg.m_enAER);
			}

			enAER = dlg.m_enAER;
		}

		switch (enAER)
		{
		case AER_REWRITE:
			Event(LS(L_REWRITINGIT), EDT_WARNING);
			if (FALSE == ::DeleteFile(m_dp.strFileName.c_str()))
			{
				DWORD dwLastError = GetLastError();
				Event(LS(L_CANTREWRITE), EDT_RESPONSE_E);
				DescribeAPIError(&dwLastError);
				Event(LS(L_WILLBERENAMED), EDT_WARNING);
				RenameFile(FALSE);
			}
			break;

		case AER_STOP:
		case AER_RESUME:
		case AER_RENAME:
		case AER_RENAME_2:
			RenameFile();
			break;
		}
	}
}

void fsDownloadMgr::AppendCommentToFileName(BOOL bMoveFile)
{
	if (m_dld == NULL || m_dld->strComment.GetLength() == 0) return;

	char szOldName[MY_MAX_PATH];
	strcpy_s(szOldName, sizeof(szOldName), m_dp.strFileName.c_str());

	LPCSTR pszExt = strrchr(szOldName, '.');
	size_t newLen = strlen(szOldName) + m_dld->strComment.GetLength() + 10 + 1;
	std::string strComment((LPCSTR)m_dld->strComment);
	while (strComment.empty() == false && strComment[0] == ' ') strComment.erase(strComment.begin());
	while (strComment.empty() == false && strComment[strComment.length() - 1] == ' ')
		strComment.erase(strComment.end() - 1);
	LPCSTR pszInvChars = ":*?\"<>|";
	for (size_t i = 0; i < strComment.length(); i++)
	{
		if (strchr(pszInvChars, strComment[i])) strComment[i] = ' ';
	}

	if (pszExt)
	{
		std::string base(szOldName, pszExt - szOldName);
		m_dp.strFileName = base + " (" + strComment + ")" + pszExt;
	}
	else
	{
		m_dp.strFileName = szOldName;
	}

	setDirty();

	if (bMoveFile)
	{
		CheckDstFileExists();

		if (FALSE == ::MoveFile(szOldName, m_dp.strFileName.c_str()))
		{
			DWORD dwLastError = GetLastError();
			Event(LS(L_CANTRENAMEBACK), EDT_RESPONSE_E);
			DescribeAPIError(&dwLastError);
		}
	}
}

void fsDownloadMgr::set_Download(fsDownload* dld)
{
	m_dld = dld;
}

BOOL fsDownloadMgr::is_GlobalOffline()
{
	DWORD dwState = 0;
	DWORD dwSize = sizeof(DWORD);
	BOOL bRet = FALSE;

	if (InternetQueryOption(NULL, INTERNET_OPTION_CONNECTED_STATE, &dwState, &dwSize))
	{
		if (dwState & INTERNET_STATE_DISCONNECTED_BY_USER) bRet = TRUE;
	}

	return bRet;
}

void fsDownloadMgr::set_GlobalOffline(BOOL bOffline)
{
	INTERNET_CONNECTED_INFO ci;
	ZeroMemory(&ci, sizeof(ci));

	if (bOffline)
	{
		ci.dwConnectedState = INTERNET_STATE_DISCONNECTED_BY_USER;
		ci.dwFlags = ISO_FORCE_DISCONNECTED;
	}
	else
	{
		ci.dwConnectedState = INTERNET_STATE_CONNECTED;
	}

	InternetSetOption(NULL, INTERNET_OPTION_CONNECTED_STATE, &ci, sizeof(ci));
}

void fsDownloadMgr::SetFileTime(HANDLE hFile)
{
	if ((m_dp.dwFlags & DPF_RETRDATEFROMSERVER) == 0) return;

	FILETIME time = m_dldr.get_FileDate();
	if (time.dwHighDateTime != 0 || time.dwLowDateTime != 0) ::SetFileTime(hFile, &time, NULL, NULL);
}

BOOL fsDownloadMgr::MoveFile(LPCSTR pszNewFileName)
{
	if (IsRunning())
	{
		SetLastError(0);
		return FALSE;
	}

	BOOL bOk = TRUE;

	if (IsFileInit())
	{
		fsBuildPathToFile(pszNewFileName);

		if (GetFileAttributes(m_dp.strFileName.c_str()) != DWORD(-1))
			bOk = ::MoveFile(m_dp.strFileName.c_str(), pszNewFileName);
		else
			bOk = TRUE;
	}

	if (bOk == FALSE) return FALSE;
	m_dp.strFileName = pszNewFileName;
	setDirty();

	return TRUE;
}

BOOL fsDownloadMgr::MoveToFolder(LPCSTR pszPath)
{
	CString str = pszPath;
	ProcessFilePathMacroses(str);

	char szFile[MY_MAX_PATH] = "";
	fsGetFileName(m_dp.strFileName.c_str(), szFile);

	char szNewFile[MY_MAX_PATH];
	lstrcpy(szNewFile, str);

	if (szNewFile[lstrlen(szNewFile) - 1] != '\\' && szNewFile[lstrlen(szNewFile) - 1] != '/') lstrcat(szNewFile, "\\");

	lstrcat(szNewFile, szFile);

	return MoveFile(szNewFile);
}

fsString fsDownloadMgr::get_URL()
{
	fsURL url;
	char szUrl[10000] = "";
	DWORD dwLen = sizeof(szUrl);

	fsDownload_NetworkProperties* dnp = GetDNP();

	url.Create(fsNPToScheme(dnp->enProtocol), dnp->strServerName.c_str(), dnp->uServerPort, NULL, NULL, dnp->strPathName.c_str(), szUrl,
	           &dwLen);

	return szUrl;
}

void fsDownloadMgr::Reset()
{
	ASSERT(IsRunning() == FALSE);
	if (IsRunning()) return;

	m_dldr.RemoveAllMirrors();
	m_dldr.ResetSections();

	m_dwDownloadFileFlags |= DFF_NEED_INIT_FILE;
	setDirty();
}

void fsDownloadMgr::InitFile_ProcessMacroses()
{
	CString str = m_dp.strFileName.c_str();

	ProcessFilePathMacroses(str);
	m_dp.strFileName = str;
	setDirty();
}

void fsDownloadMgr::ProcessFilePathMacroses(CString& str)
{
	if (str.Find('%', 0) == -1) return;

	if (str.Find("%sdrive%") != -1)
	{
		str.Replace("%sdrive%", CString(vmsGetExeDriveLetter()) + ":");
		m_dwDownloadFileFlags |= DFF_USE_PORTABLE_DRIVE;
		setDirty();
	}

	str.Replace("%server%", GetDNP()->strServerName.c_str());

	char szUrlPath[MY_MAX_PATH];
	fsGetPath(GetDNP()->strPathName.c_str(), szUrlPath);
	if (lstrlen(szUrlPath) > 1)
		str.Replace("%path_on_server%", szUrlPath);
	else
		str.Replace("%path_on_server%", "");
	str.Replace("/", "\\");
	str.Replace("\\\\", "\\");

	SYSTEMTIME st;
	GetLocalTime(&st);

	str.Replace("%date%", "%year%-%month%-%day%");

	CString strY, strM, strD;
	strY.Format("%04d", (int)st.wYear);
	strM.Format("%02d", (int)st.wMonth);
	strD.Format("%02d", (int)st.wDay);

	str.Replace("%year%", strY);
	str.Replace("%month%", strM);
	str.Replace("%day%", strD);

	TCHAR szPath[MAX_PATH] = {
	    0,
	};
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, szPath))) str.Replace("%userprofile%", szPath);
}

int fsDownloadMgr::get_ReservingDiskSpaceProgress()
{
	return 0;
}

DWORD fsDownloadMgr::get_State()
{
	return m_dwState;
}

BOOL fsDownloadMgr::HasActivity()
{
	return m_iThread != 0;
}

DWORD WINAPI fsDownloadMgr::_threadReserveDiskSpace(LPVOID lp)
{
	fsDownloadMgr* pthis = (fsDownloadMgr*)lp;

	pthis->setStateFlags(DS_RESERVINGSPACE);

	ASSERT(pthis->m_dldr.GetNumberOfSections() == 1);
	ASSERT(pthis->m_dldr.GetLDFileSize() != _UI64_MAX);

	UINT64 uStartPos = pthis->m_dldr.GetDownloadedBytesCount() + 100 * 1024 * 1024;

	UINT64 uHundredthPart = pthis->m_dldr.GetLDFileSize() / 100;

	fsTicksMgr timeStart;
	bool bEvent = true;

	for (UINT64 uCurPos = uStartPos; uCurPos < pthis->m_dldr.GetLDFileSize(); uCurPos += uHundredthPart)
	{
		UINT64 uSectLastDownloadPos = pthis->m_dldr.GetDownloadedBytesCount();

		pthis->m_dldr.LockWriteFile(TRUE);

		if (pthis->m_dldr.GetDownloadedBytesCount() < uCurPos)
		{
			if (!fsSetFilePointer(pthis->m_hOutFile, uCurPos, FILE_BEGIN))
			{
				pthis->m_dldr.LockWriteFile(FALSE);
				break;
			}

			BYTE b = 1;
			DWORD dw;
			if (FALSE == WriteFile(pthis->m_hOutFile, &b, sizeof(b), &dw, NULL))
			{
				pthis->m_dldr.LockWriteFile(FALSE);
				break;
			}
		}

		pthis->m_dldr.LockWriteFile(FALSE);

		if (pthis->m_dldr.GetDownloadedBytesCount() == uSectLastDownloadPos)
		{

			Sleep(50);
		}

		if (pthis->m_dldr.IsRunning() == FALSE) break;

		fsTicksMgr timeNow;
		if (bEvent && timeNow - timeStart > 3 * 1000)
		{
			bEvent = false;
			pthis->Event(LS(L_PREP_FILES_ONDISK));
		}
	}

	pthis->removeStateFlags(DS_RESERVINGSPACE);

	pthis->m_bDontCreateNewSections = FALSE;
	pthis->setStateFlags(DS_NEEDADDSECTION);

	InterlockedDecrement(&pthis->m_iThread);
	return 0;
}

fsString fsDownloadMgr::getFileName()
{
	char szFile[MY_MAX_PATH] = "";
	LPCSTR pszSuggFile = m_dldr.GetSuggestedFileName();
	if (pszSuggFile && *pszSuggFile)
	{
		strcpy_s(szFile, sizeof(szFile), pszSuggFile);
	}
	else
	{
		fsFileNameFromUrlPath(GetDNP()->strPathName.c_str(), GetDNP()->enProtocol == NP_FTP, TRUE, szFile, sizeof(szFile));
	}
	if (*szFile == 0) fsGetFileName(GetDP()->strFileName.c_str(), szFile);
	return szFile;
}

void fsDownloadMgr::getObjectItselfStateBuffer(LPBYTE pb, LPDWORD pdwSize, bool bSaveToStorage)
{
	DWORD dwSectionsSize;
	if (FALSE == m_dldr.SaveSectionsState(NULL, &dwSectionsSize)) return;

	// Calculate total size needed
	DWORD dwNeedSize = sizeof(DWORD) + dwSectionsSize; // sections state
	dwNeedSize += SerializedDP_NonStringSize();
	dwNeedSize += SerializedDNP_NonStringSize();
	dwNeedSize += sizeof(m_dwState);
	dwNeedSize += sizeof(m_dwDownloadFileFlags);
	dwNeedSize += sizeof(int); // mirror count

	// DP strings
	dwNeedSize += SerializedStringSize(m_dp.strFileName);
	dwNeedSize += SerializedStringSize(m_dp.strAdditionalExt);
	dwNeedSize += SerializedStringSize(m_dp.strCreateExt);
	dwNeedSize += SerializedStringSize(m_dp.strCheckSum);

	// DNP strings (base)
	fsDownload_NetworkProperties* dnp = GetDNP();
	dwNeedSize += SerializedDNP_StringsSize(*dnp);

	// Mirrors
	int cMirrs = m_dldr.GetMirrorURLCount();
	for (int mi = 0; mi < cMirrs; mi++)
	{
		dwNeedSize += SerializedDNP_NonStringSize();
		dwNeedSize += sizeof(BOOL); // bIsGood
		dwNeedSize += SerializedDNP_StringsSize(*m_dldr.MirrorDNP(mi));
	}

	if (cMirrs)
	{
		dwNeedSize += cMirrs * sizeof(DWORD); // ping times
		dwNeedSize += sizeof(DWORD); // base server ping
	}

	if (pb == NULL)
	{
		*pdwSize = dwNeedSize;
		return;
	}

	if (*pdwSize < dwNeedSize)
	{
		*pdwSize = dwNeedSize;
		return;
	}

	LPBYTE pB = pb;

	// Sections state
	DWORD dw = *pdwSize - sizeof(DWORD);
	if (FALSE == m_dldr.SaveSectionsState(pB + sizeof(DWORD), &dw)) return;
	CopyMemory(pB, &dw, sizeof(DWORD));
	pB += sizeof(DWORD) + dw;

	// DP non-string fields
	SerializeDP_NonString(m_dp, pB);

	// DNP non-string fields
	SerializeDNP_NonString(*dnp, pB);

	// State
	typedef DWORD fsDownloadState;
	fsDownloadState state = (m_dwState & DS_DONE) ? DS_DONE : 0;
	CopyMemory(pB, &state, sizeof(state));
	pB += sizeof(state);

	// Download file flags
	if ((m_dwDownloadFileFlags & DFF_USE_PORTABLE_DRIVE) && !m_dp.strFileName.empty() && m_dp.strFileName[0] != vmsGetExeDriveLetter())
		m_dwDownloadFileFlags &= ~DFF_USE_PORTABLE_DRIVE;
	CopyMemory(pB, &m_dwDownloadFileFlags, sizeof(m_dwDownloadFileFlags));
	pB += sizeof(m_dwDownloadFileFlags);

	// Mirror count
	CopyMemory(pB, &cMirrs, sizeof(cMirrs));
	pB += sizeof(cMirrs);

	// DP strings
	SerializeString(m_dp.strFileName, pB);
	SerializeString(m_dp.strAdditionalExt, pB);
	SerializeString(m_dp.strCreateExt, pB);
	SerializeString(m_dp.strCheckSum, pB);

	// DNP strings (base)
	SerializeDNP_Strings(*dnp, pB);

	// Mirrors
	for (int i = 0; i < cMirrs; i++)
	{
		fsDownload_NetworkProperties* mirrDnp = m_dldr.MirrorDNP(i);
		SerializeDNP_NonString(*mirrDnp, pB);

		BOOL b = m_dldr.GetMirrorIsGood(i);
		CopyMemory(pB, &b, sizeof(b));
		pB += sizeof(b);

		SerializeDNP_Strings(*mirrDnp, pB);
	}

	// Mirror ping times
	if (cMirrs)
	{
		for (int i = 0; i < cMirrs; i++)
		{
			DWORD _dw = m_dldr.GetMirrorPingTime(i);
			CopyMemory(pB, &_dw, sizeof(_dw));
			pB += sizeof(_dw);
		}

		DWORD _dw = m_dldr.Get_BaseServerPingTime();
		CopyMemory(pB, &_dw, sizeof(_dw));
		pB += sizeof(_dw);
	}

	*pdwSize = (DWORD)(pB - pb);
}

bool fsDownloadMgr::loadObjectItselfFromStateBuffer(LPBYTE pb, LPDWORD pdwSize, DWORD dwVer)
{
	if (dwVer <= 16) return false; // Only support new format (ver 17+)

	LPBYTE pB = pb;
	LPBYTE pEnd = pb + *pdwSize;

	// Sections state
	if (pB + sizeof(DWORD) > pEnd) return false;
	DWORD dw;
	CopyMemory(&dw, pB, sizeof(DWORD));
	pB += sizeof(DWORD);
	if (pB + dw > pEnd) return false;
	if (FALSE == m_dldr.RestoreSectionsState(pB, dw, dwVer)) return false;
	pB += dw;

	// DP non-string fields
	if (!DeserializeDP_NonString(m_dp, pB, pEnd)) return false;

	// DNP non-string fields
	fsDownload_NetworkProperties* dnp = GetDNP();
	if (!DeserializeDNP_NonString(*dnp, pB, pEnd)) return false;

	// State
	if (pB + sizeof(m_dwState) > pEnd) return false;
	CopyMemory(&m_dwState, pB, sizeof(m_dwState));
	pB += sizeof(m_dwState);

	// Download file flags
	if (pB + sizeof(m_dwDownloadFileFlags) > pEnd) return false;
	CopyMemory(&m_dwDownloadFileFlags, pB, sizeof(m_dwDownloadFileFlags));
	pB += sizeof(m_dwDownloadFileFlags);

	// Mirror count
	if (pB + sizeof(int) > pEnd) return false;
	int cMirrs = 0;
	CopyMemory(&cMirrs, pB, sizeof(int));
	pB += sizeof(int);

	// DP strings
	if (!DeserializeString(m_dp.strFileName, pB, pEnd)) return false;
	if (m_dwDownloadFileFlags & DFF_USE_PORTABLE_DRIVE && !m_dp.strFileName.empty())
		m_dp.strFileName[0] = vmsGetExeDriveLetter();
	if (!DeserializeString(m_dp.strAdditionalExt, pB, pEnd)) return false;
	if (!DeserializeString(m_dp.strCreateExt, pB, pEnd)) return false;
	if (!DeserializeString(m_dp.strCheckSum, pB, pEnd)) return false;

	// DNP strings (base + mirrors)
	for (int i = 0; i < cMirrs + 1; i++)
	{
		fsDownload_NetworkProperties tmpdnp;
		BOOL bMirrIsGood = TRUE;

		if (i)
		{
			if (!DeserializeDNP_NonString(tmpdnp, pB, pEnd)) return false;
			if (pB + sizeof(BOOL) > pEnd) return false;
			CopyMemory(&bMirrIsGood, pB, sizeof(BOOL));
			pB += sizeof(BOOL);
			dnp = &tmpdnp;
		}

		if (!DeserializeDNP_Strings(*dnp, pB, pEnd)) return false;

		if (i) m_dldr.AddMirror(dnp, TRUE, TRUE);
	}

	// Mirror ping times
	if (cMirrs)
	{
		for (int i = 0; i < cMirrs; i++)
		{
			if (pB + sizeof(DWORD) > pEnd) return false;
			DWORD _dw;
			CopyMemory(&_dw, pB, sizeof(DWORD));
			pB += sizeof(DWORD);
			m_dldr.Set_MirrPingTime(i, _dw);
		}

		if (pB + sizeof(DWORD) > pEnd) return false;
		DWORD _dw;
		CopyMemory(&_dw, pB, sizeof(DWORD));
		pB += sizeof(DWORD);
		m_dldr.Set_BaseServerPingTime(_dw);
	}

	*pdwSize = (DWORD)(pB - pb);
	return true;
}

UINT64 fsDownloadMgr::getSpeed(bool bOfDownload)
{
	return bOfDownload ? GetDownloader()->GetSpeed() : 0;
}

void fsDownloadMgr::setSpeedLimit(bool bForDownload, UINT64 uLimit)
{
	UINT uLimitUINT = uLimit == UINT64_MAX ? UINT_MAX : (UINT)uLimit;
	if (bForDownload) GetDownloader()->LimitTraffic(uLimitUINT);
}

UINT64 fsDownloadMgr::getSpeedLimit(bool bOfDownload)
{
	UINT uLimit = bOfDownload ? GetDownloader()->GetTrafficLimit() : UINT_MAX;
	return uLimit == UINT_MAX ? UINT64_MAX : uLimit;
}

bool fsDownloadMgr::isResumeSupported(void)
{
	return GetDownloader()->IsResumeSupported() != RST_NONE;
}

bool fsDownloadMgr::isNoSpeedLimit(bool bOfDownload)
{
	return bOfDownload ? GetDP()->bIgnoreRestrictions != FALSE : true;
}

UINT64 fsDownloadMgr::getInternalSpeedLimit(bool bOfDownload)
{
	return bOfDownload && GetDP()->uTrafficRestriction != UINT_MAX ? GetDP()->uTrafficRestriction : _UI64_MAX;
}

bool fsDownloadMgr::isRequiresTraffic(bool bForDownload)
{
	return bForDownload;
}

bool fsDownloadMgr::isSpeedCanBeLimitedBySomeInternalReasons(bool bForDownload)
{
	return (m_dwState & DS_RESERVINGSPACE) != 0;
}

bool fsDownloadMgr::isInternetTraffic(bool bForDownload)
{
	return GetDNP()->enProtocol != NP_FILE && (m_dldr.GetState() & IDS_MIRRORS_HAS_LOCAL_SOURCE) == 0;
}

void fsDownloadMgr::setStateFlags(DWORD dwFlags)
{
	vmsAUTOLOCKSECTION(m_csState);
	m_dwState |= dwFlags;
	setDirty();
}

void fsDownloadMgr::setStateFlagsTo(DWORD dwFlags)
{
	vmsAUTOLOCKSECTION(m_csState);
	m_dwState = dwFlags;
	setDirty();
}

void fsDownloadMgr::removeStateFlags(DWORD dwFlags)
{
	vmsAUTOLOCKSECTION(m_csState);
	m_dwState &= ~dwFlags;
	setDirty();
}

bool fsDownloadMgr::IsFailedToCreateDestinationFile() const
{
	return m_bFailedToCreateDestinationFile;
}

bool fsDownloadMgr::IsNotEnoughDiskSpace() const
{
	return m_bIsNotEnoughDiskSpace;
}
