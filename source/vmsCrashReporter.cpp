/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "vmsCrashReporter.h"
#include "vmsXmlHelper.h"

vmsCrashReporter::vmsCrashReporter(void) {}

vmsCrashReporter::~vmsCrashReporter(void) {}

void vmsCrashReporter::GenerateXml(LPCTSTR ptszAppName, LPCTSTR ptszVersion, LPCTSTR ptszDescription,
                                   LPCTSTR ptszFaultModule, DWORD_PTR dwpCrashAddr, LPCSTR pszAdditionalXmlData,
                                   std::string& strResult)
{
	strResult = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";

	strResult += "<CrashDumpInfo>\r\n";

	strResult += "<Application Name=\"";
	strResult += vmsXmlHelper::toUtf8(ptszAppName);
	strResult += "\" Version=\"";
	strResult += vmsXmlHelper::toUtf8(ptszVersion);
	strResult += "\" Platform=\"";
#ifdef _WIN64
	strResult += "x64";
#else
	strResult += "x32";
#endif
	strResult += "\" />\r\n";

	strResult += "<Description>";
	strResult += vmsXmlHelper::toUtf8(ptszDescription);
	strResult += "</Description>\r\n";

	if (ptszFaultModule)
	{
		strResult += "<FaultModule Name=\"";
		strResult += vmsXmlHelper::toUtf8(ptszFaultModule);
		strResult += "\" Address=\"0x";
		char sz[100] = "";
		sprintf_s<100>(sz, "%I64x", (UINT64)dwpCrashAddr);
		strResult += sz;
		strResult += "\" />\r\n";
	}

	if (pszAdditionalXmlData && *pszAdditionalXmlData) strResult += pszAdditionalXmlData;

	strResult += "</CrashDumpInfo>\r\n";
}