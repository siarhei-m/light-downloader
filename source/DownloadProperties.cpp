/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "DownloadProperties.h"
#include "../hash/vmsHash.h"

void fsDNP_GetDefaults(fsDownload_NetworkProperties* pDNP)
{
	if (pDNP == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return;
	}

	pDNP->wRollBackSize = _App.RollBackSize();
	pDNP->enAccType = _App.InternetAccessType();
	pDNP->dwFtpFlags = _App.FtpFlags();
	pDNP->bUseHttp11 = _App.UseHttp11();
	pDNP->enFtpTransferType = _App.FtpTransferType();
	pDNP->bUseCookie = _App.UseCookie();
	pDNP->dwFlags = _App.DNPFlags();
	pDNP->wLowSpeed_Duration = _App.LowSpeed_Duration();
	pDNP->wLowSpeed_Factor = _App.LowSpeed_Factor();

	pDNP->strAgent = _App.Agent();
	pDNP->strPassword = _App.UserPassword();
	pDNP->strReferer = _App.Referer();
	pDNP->strUserName = _App.UserName();
	pDNP->strASCIIExts = _App.ASCIIExts();
	pDNP->strCookies = "";
	pDNP->strPostData = "";
}

void fsDP_GetDefaults(fsDownload_Properties* pDP)
{
	if (pDP == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return;
	}

	pDP->bIgnoreRestrictions = _App.IgnoreRestrictions();
	pDP->bRestartSpeedLow = _App.RestartSpeedLow();
	pDP->uMaxAttempts = _App.MaxAttempts();
	pDP->uMaxSections = _App.MaxSections();
	pDP->uSectionMinSize = _App.SectionMinSize();
	pDP->uTrafficRestriction = _App.TrafficRestriction();
	pDP->uRetriesTime = _App.RetriesTime();
	pDP->bReserveDiskSpace = _App.ReserveDiskSpace();
	pDP->uTimeout = _App.Timeout();

	pDP->aEP[DFE_NOTFOUND] = _App.NotFoundReaction();
	pDP->aEP[DFE_ACCDENIED] = _App.AccDeniedReaction();

	pDP->enAER = _App.AlreadyExistReaction();
	pDP->enSCR = _App.SizeChangeReaction();

	pDP->dwFlags = _App.DownloadFlags();

	pDP->bCheckIntegrityWhenDone = _App.Download_CheckIntegrityWhenDone();
	pDP->dwIntegrityCheckAlgorithm = HA_MD5;
	pDP->enICFR = (vmsIntegrityCheckFailedReaction)_App.Download_IntegrityCheckFailedReaction();
	pDP->strCheckSum = "";

	pDP->strAdditionalExt = _App.AdditionalExtension();
	pDP->strCreateExt = _App.Download_CreateExt();
}

fsInternetResult fsDNP_GetByUrl(fsDownload_NetworkProperties* pDNP, LPCSTR pszUrl)
{
	fsDNP_GetDefaults(pDNP);

	fsURL url;

	fsInternetResult ir = url.Crack(pszUrl);
	if (ir != IR_SUCCESS)
		return ir;

	pDNP->enProtocol = fsSchemeToNP(url.GetInternetScheme());

	fsGetProxyByNP(pDNP);

	pDNP->uServerPort = url.GetPort();

	pDNP->strServerName = url.GetHostName();
	pDNP->strPathName = url.GetPath();

	LPCSTR pszUser = url.GetUserName();
	LPCSTR pszPass = url.GetPassword();

	if (*pszUser)
		pDNP->strUserName = pszUser;

	if (*pszPass)
		pDNP->strPassword = pszPass;

	return IR_SUCCESS;
}

void fsDNP_SetAuth(fsDownload_NetworkProperties* dnp, LPCSTR pszUser, LPCSTR pszPassword)
{
	if (pszUser)
		dnp->strUserName = pszUser;
	else
		dnp->strUserName.clear();

	if (pszPassword)
		dnp->strPassword = pszPassword;
	else
		dnp->strPassword.clear();
}

fsInternetResult fsDNP_ApplyUrl(fsDownload_NetworkProperties* dnp, LPCSTR pszUrl)
{
	fsURL url;
	fsInternetResult ir;

	ir = url.Crack(pszUrl);
	if (ir != IR_SUCCESS) return ir;

	fsNetworkProtocol np = fsSchemeToNP(url.GetInternetScheme());

	if (dnp->enProtocol != np)
	{
		dnp->strProxyName.clear();
		dnp->strProxyUserName.clear();
		dnp->strProxyPassword.clear();

		dnp->enProtocol = np;

		fsGetProxyByNP(dnp);
	}

	dnp->uServerPort = url.GetPort();

	dnp->strPathName = url.GetPath();
	dnp->strServerName = url.GetHostName();

	if (*url.GetUserName())
	{
		dnp->strUserName = url.GetUserName();
		dnp->strPassword = url.GetPassword();
	}
	else
	{
		dnp->strUserName.clear();
		dnp->strPassword.clear();
	}

	return IR_SUCCESS;
}

fsNetworkProtocol fsSchemeToNP(INTERNET_SCHEME scheme)
{
	switch (scheme)
	{
	case INTERNET_SCHEME_FTP:
		return NP_FTP;

	case INTERNET_SCHEME_HTTP:
		return NP_HTTP;

	case INTERNET_SCHEME_HTTPS:
		return NP_HTTPS;

	case INTERNET_SCHEME_FILE:
		return NP_FILE;
	}

	ASSERT(FALSE);
	return (fsNetworkProtocol)-1;
}

INTERNET_SCHEME fsNPToScheme(fsNetworkProtocol np)
{
	switch (np)
	{
	case NP_HTTP:
		return INTERNET_SCHEME_HTTP;

	case NP_HTTPS:
		return INTERNET_SCHEME_HTTPS;

	case NP_FTP:
		return INTERNET_SCHEME_FTP;

	case NP_FILE:
		return INTERNET_SCHEME_FILE;

	default:
		ASSERT(0);
		return INTERNET_SCHEME_UNKNOWN;
	}
}

fsInternetResult fsGetProxyByNP(fsDownload_NetworkProperties* pDNP)
{
	switch (pDNP->enProtocol)
	{
	case NP_FTP:
		pDNP->strProxyName = _App.FtpProxy_Name();
		pDNP->strProxyPassword = _App.FtpProxy_Password();
		pDNP->strProxyUserName = _App.FtpProxy_UserName();
		break;

	case NP_HTTP:
		pDNP->strProxyName = _App.HttpProxy_Name();
		pDNP->strProxyPassword = _App.HttpProxy_Password();
		pDNP->strProxyUserName = _App.HttpProxy_UserName();
		break;

	case NP_HTTPS:
		pDNP->strProxyName = _App.HttpsProxy_Name();
		pDNP->strProxyPassword = _App.HttpsProxy_Password();
		pDNP->strProxyUserName = _App.HttpsProxy_UserName();
		break;

	case NP_FILE:
		pDNP->strProxyName.clear();
		pDNP->strProxyPassword.clear();
		pDNP->strProxyUserName.clear();
		break;

	default:
		return IR_BADURL;
	}

	return IR_SUCCESS;
}

BOOL fsGetProxy(fsNetworkProtocol np, CString& strProxy, CString& strUser, CString& strPassword)
{
	switch (np)
	{
	case NP_HTTP:
		strProxy = _App.HttpProxy_Name();
		strUser = _App.HttpProxy_UserName();
		strPassword = _App.HttpProxy_Password();
		break;

	case NP_HTTPS:
		strProxy = _App.HttpsProxy_Name();
		strUser = _App.HttpsProxy_UserName();
		strPassword = _App.HttpsProxy_Password();
		break;

	case NP_FTP:
		strProxy = _App.FtpProxy_Name();
		strUser = _App.FtpProxy_UserName();
		strPassword = _App.FtpProxy_Password();
		break;

	case NP_FILE:
		strProxy = "";
		strUser = "";
		strPassword = "";
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

BOOL fsIsSameProtocols(fsNetworkProtocol np1, fsNetworkProtocol np2)
{
	return np1 == np2 || ((np1 == NP_HTTP || np1 == NP_HTTPS) && (np2 == NP_HTTP || np2 == NP_HTTPS));
}

BOOL fsDNP_CloneSettings(fsDownload_NetworkProperties* dst, fsDownload_NetworkProperties* src)
{
	if (fsIsSameProtocols(src->enProtocol, dst->enProtocol) == FALSE) return FALSE;

	dst->wRollBackSize = src->wRollBackSize;
	dst->enAccType = src->enAccType;
	dst->dwFtpFlags = src->dwFtpFlags;
	dst->bUseHttp11 = src->bUseHttp11;
	dst->enFtpTransferType = src->enFtpTransferType;
	dst->bUseCookie = src->bUseCookie;

	dst->strAgent = src->strAgent;
	dst->strASCIIExts = src->strASCIIExts;

	dst->strProxyName = src->strProxyName;
	dst->strProxyUserName = src->strProxyUserName;
	dst->strProxyPassword = src->strProxyPassword;

	dst->dwFlags = src->dwFlags;

	return TRUE;
}

void fsDNP_GetURL(fsDownload_NetworkProperties* dnp, LPSTR pszURL)
{
	DWORD dw = 10000;
	fsURL url;

	if (IR_SUCCESS != url.Create(fsNPToScheme(dnp->enProtocol), dnp->strServerName.c_str(), dnp->uServerPort, dnp->strUserName.c_str(),
	                             dnp->strPassword.c_str(), dnp->strPathName.c_str(), pszURL, &dw))
		*pszURL = 0;
}
