/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "vmsXmlHelper.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

vmsXmlHelper::vmsXmlHelper() {}

vmsXmlHelper::~vmsXmlHelper() {}

bool vmsXmlHelper::LoadXML(pugi::xml_document& doc, LPCSTR pszXml)
{
	pugi::xml_parse_result result = doc.load_string(pszXml);
	return result;
}

std::string vmsXmlHelper::toUtf8(const tstring& tstr, bool bRemoveBadChars)
{
	CString str = tstr.c_str();

	if (bRemoveBadChars)
	{
		for (int i = 0; i < str.GetLength(); i++)
		{
			if (str[i] >= ' ') continue;
			char ch = str[i];
			if (ch == '\t' || ch == '\r' || ch == '\n') continue;
			str.Delete(i--);
		}
	}

	str.Replace(_T ("&"), _T ("&amp;"));
	str.Replace(_T ("<"), _T ("&lt;"));
	str.Replace(_T (">"), _T ("&gt;"));
	str.Replace(_T ("'"), _T ("&apos;"));
	str.Replace(_T ("\""), _T ("&quot;"));

	wchar_t* pwsz = new wchar_t[str.GetLength() * 2 + 1];

	wchar_t* pwsz2 = new wchar_t[str.GetLength() * 2 + 1];

#if defined(UNICODE) || defined(_UNICODE)
	wcscpy(pwsz, str);
#else
	if (0 == MultiByteToWideChar(CP_ACP, 0, str, -1, pwsz, str.GetLength() * 2 + 1))
	{
		delete[] pwsz;
		delete[] pwsz2;
		int iRL = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
		if (!iRL) throw "unicode conversion failed";
		pwsz = new wchar_t[++iRL];
		pwsz2 = new wchar_t[iRL];
		if (0 == MultiByteToWideChar(CP_ACP, 0, str, -1, pwsz, iRL)) throw "unicode conversion failed";
	}
#endif

	int len = wcslen(pwsz);

	int i = 0, j = 0;
	for (i = 0, j = 0; i < len; i++)
	{
		wchar_t wch = pwsz[i];
		if (wch == 0x9 || wch == 0xA || wch == 0xD || ((wch >= 0x20) && (wch <= 0xD7FF)) ||
		    ((wch >= 0xE000) && (wch <= 0xFFFD)) || ((wch >= 0x10000) && (wch <= 0x10FFFF)))
		{
			pwsz2[j++] = wch;
		}
	}

	pwsz2[j] = 0;

	LPSTR psz = new char[6 * len + 1];
	if (0 == WideCharToMultiByte(CP_UTF8, 0, pwsz2, -1, psz, 6 * len + 1, NULL, NULL))
	{
		delete[] psz;
		int iRL = WideCharToMultiByte(CP_UTF8, 0, pwsz2, -1, NULL, 0, NULL, NULL);
		if (!iRL) throw "utf-8 encode error";
		psz = new char[++iRL];
		if (0 == WideCharToMultiByte(CP_UTF8, 0, pwsz2, -1, psz, iRL, NULL, NULL)) throw "utf-8 encode error";
	}

	std::string str2 = psz;

	delete[] pwsz2;
	delete[] pwsz;
	delete[] psz;

	return str2;
}

tstring vmsXmlHelper::FromUtf8(LPCSTR psz)
{
	wchar_t* pwsz = new wchar_t[strlen(psz) + 1];

	MultiByteToWideChar(CP_UTF8, 0, psz, -1, pwsz, strlen(psz) + 1);

	USES_CONVERSION;
	tstring tstr = W2T(pwsz);

	delete[] pwsz;

	return tstr;
}

std::string vmsXmlHelper::toUtf8noEncode(const tstring& tstr)
{
#if defined(UNICODE) || defined(_UNICODE)
	wchar_t* pwsz = new wchar_t[tstr.length() + 1];
	wcscpy(pwsz, tstr.c_str());
#else
	int iRL = MultiByteToWideChar(CP_ACP, 0, tstr.c_str(), -1, NULL, 0);
	wchar_t* pwsz = new wchar_t[++iRL];
	MultiByteToWideChar(CP_ACP, 0, tstr.c_str(), -1, pwsz, iRL);
#endif

	int iRL2 = WideCharToMultiByte(CP_UTF8, 0, pwsz, -1, NULL, 0, NULL, NULL);
	LPSTR psz = new char[iRL2];
	WideCharToMultiByte(CP_UTF8, 0, pwsz, -1, psz, iRL2, NULL, NULL);

	std::string str2 = psz;

	delete[] pwsz;
	delete[] psz;

	return str2;
}
