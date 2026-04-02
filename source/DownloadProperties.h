/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#ifndef __DOWNLOADPROPERTIES_H_
#define __DOWNLOADPROPERTIES_H_

#include "../inetfile/InetFile.h"
#include <string>

enum fsNetworkProtocol
{
	NP_HTTP,
	NP_HTTPS,
	NP_FTP,
	NP_FILE
};

#define FTP_USEPASSIVEMODE 1

#define FTP_DONTSENDLIST 2

enum fsInternetAccessTypeEx
{
	IATE_NOPROXY = IAT_NOPROXY,
	// IATE_PRECONFIGPROXY	= IAT_PRECONFIGPROXY,
	IATE_MANUALPROXY = IAT_MANUALPROXY,
	// IATE_FIREFOXPROXY,
	IATE_SOCKS5PROXY = IAT_SOCKS5PROXY // VS
};

struct fsDownload_NetworkProperties
{
	WORD wRollBackSize = 0;

	std::string strAgent;

	fsInternetAccessTypeEx enAccType = IATE_NOPROXY;
	std::string strProxyName;
	std::string strProxyUserName;
	std::string strProxyPassword;

	fsNetworkProtocol enProtocol = NP_HTTP;
	std::string strServerName;
	INTERNET_PORT uServerPort = 0;
	std::string strUserName;
	std::string strPassword;

	std::string strPathName;

	BOOL bUseHttp11 = FALSE;
	std::string strReferer;
	BOOL bUseCookie = FALSE;

	DWORD dwFtpFlags = 0;
	fsFtpTransferType enFtpTransferType = FTT_UNKNOWN;

	std::string strASCIIExts;

	std::string strCookies;
	std::string strPostData;

	DWORD dwFlags = 0;

	WORD wLowSpeed_Factor = 0;

	WORD wLowSpeed_Duration = 0;
};

#define DNPF_RESTARTSECTIONWHENSPEEDTOOLOW 1

#define DNPF_IMMEDIATELY_SEND_AUTH_AS_BASIC (1 << 1)

#define DNPF_DONT_UPDATE_ORIGINAL_URL_AFTER_REDIRECT (1 << 2)

// fsDNP_BuffersInfo removed - no longer needed with std::string

enum fsDownloadFileError
{
	DFE_NOTFOUND = 0,
	DFE_ACCDENIED = 1,

	DFE_UNKNOWN = 2
};

enum fsDownloadFileErrorProcessing
{
	DFEP_STOP,
	DFEP_IGNORE
};

enum fsAlreadyExistReaction
{
	AER_ASKUSER,
	AER_REWRITE,
	AER_RENAME,
	AER_RESUME,
	AER_STOP,
	AER_RENAME_2
};

enum fsSizeChangeReaction
{
	SCR_ASKUSER,
	SCR_RESTART,
	SCR_ADJUSTFORNEWSIZE,
	SCR_STOP,
};

enum vmsIntegrityCheckFailedReaction
{
	ICFR_ASKUSER,
	ICFR_RESTART,
	ICFR_IGNORE,
};

#define DPF_DONTRESTARTIFNORESUME 1
#define DPF_USEZIPPREVIEW 2

#define DPF_USEHIDDENATTRIB 4

#define DPF_APPENDCOMMENTTOFILENAME 8

#define DPF_STARTWHENDONE 16

#define DPF_STARTWHENDONE_NOCONF 32

#define DPF_RETRDATEFROMSERVER 64

#define DPF_GENERATEDESCFILE 128

struct fsDownload_Properties
{
	WORD wStructSize = 0;

	UINT uTrafficRestriction = 0;
	UINT uMaxAttempts = 0;
	UINT uRetriesTime = 0;
	UINT uTimeout = 0;

	UINT uSectionMinSize = 0;
	UINT uMaxSections = 0;
	BOOL bRestartSpeedLow = FALSE;

	std::string strFileName;
	BOOL bReserveDiskSpace = FALSE;

	BOOL bIgnoreRestrictions = FALSE;

	fsDownloadFileErrorProcessing aEP[DFE_UNKNOWN] = {};
	fsAlreadyExistReaction enAER = AER_ASKUSER;
	fsSizeChangeReaction enSCR = SCR_ASKUSER;
	std::string strAdditionalExt;
	DWORD dwFlags = 0;
	std::string strCreateExt;

	BOOL bCheckIntegrityWhenDone = FALSE;
	vmsIntegrityCheckFailedReaction enICFR = ICFR_ASKUSER;
	std::string strCheckSum;

	DWORD dwIntegrityCheckAlgorithm = 0;
};

// fsDP_BuffersInfo removed - no longer needed with std::string

extern void fsDNP_SetAuth(fsDownload_NetworkProperties* dnp, LPCSTR pszUser, LPCSTR pszPassword);

extern void fsDNP_GetDefaults(fsDownload_NetworkProperties* pDNP);

// fsDNP_GetDefaults_Free removed - std::string auto-manages memory

extern fsInternetResult fsDNP_ApplyUrl(fsDownload_NetworkProperties* dnp, LPCSTR pszUrl);

extern void fsDP_GetDefaults(fsDownload_Properties* pDP);

extern fsInternetResult fsDNP_GetByUrl(fsDownload_NetworkProperties* pDNP, LPCSTR pszUrl);

// fsDNP_GetByUrl_Free removed - std::string auto-manages memory

// TEST
// extern fsInternetResult fsDNP_GetSocks(fsDownload_NetworkProperties *pDNP, fsDNP_BuffersInfo* pBuffs, BOOL
// bAllocate);

extern fsNetworkProtocol fsSchemeToNP(INTERNET_SCHEME scheme);
extern INTERNET_SCHEME fsNPToScheme(fsNetworkProtocol np);

extern fsInternetResult fsGetProxyByNP(fsDownload_NetworkProperties* pDNP);

extern BOOL fsGetProxy(fsNetworkProtocol np, CString& strProxy, CString& strUser, CString& strPassword);

extern BOOL fsDNP_CloneSettings(fsDownload_NetworkProperties* dst, fsDownload_NetworkProperties* src);

extern void fsDNP_GetURL(fsDownload_NetworkProperties* dnp, LPSTR pszURL);

#endif