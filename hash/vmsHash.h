/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#pragma once

#include <windows.h>
#include <bcrypt.h>
#pragma warning(push, 3)
#include "vmsHashEvents.h"
#include <string>
#pragma warning(pop)

enum vmsHashAlgorithm
{
	HA_MD5,
	HA_SHA1,
	HA_SHA2,
};

enum vmsHash_SHA2Strength
{
	HSHA2S_256,
};

class vmsHash
{
  public:
	void set_SHA2Strength(vmsHash_SHA2Strength en);
	void set_EventsHandler(vmsHashEvents* pEvents);
	std::string Hash(LPCSTR pszFile, vmsHashAlgorithm enHA = HA_MD5);

	vmsHash();
	virtual ~vmsHash();

  protected:
	std::string HashFile(LPCSTR pszFile, LPCWSTR pszAlgId, int nDigestSize);
	static std::string DigestToHex(const BYTE* pDigest, int nSize);
	vmsHashEvents* m_pEvents;
	int m_nSHA2Strength;
};
