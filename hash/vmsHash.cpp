/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
  Updated 2026: Replaced hand-rolled crypto with Windows BCrypt/CNG API.
*/

#include "vmsHash.h"
#include "vmsFile.h"
#include <vector>

#pragma comment(lib, "bcrypt.lib")

vmsHash::vmsHash()
{
	m_pEvents = NULL;
	m_nSHA2Strength = 256;
}

vmsHash::~vmsHash() {}

std::string vmsHash::Hash(LPCSTR pszFile, vmsHashAlgorithm enHA)
{
	switch (enHA)
	{
	case HA_MD5:
		return HashFile(pszFile, BCRYPT_MD5_ALGORITHM, 16);
	case HA_SHA1:
		return HashFile(pszFile, BCRYPT_SHA1_ALGORITHM, 20);
	case HA_SHA2:
		return HashFile(pszFile, BCRYPT_SHA256_ALGORITHM, 32);
	default:
		return "";
	}
}

std::string vmsHash::HashFile(LPCSTR pszFile, LPCWSTR pszAlgId, int nDigestSize)
{
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_HASH_HANDLE hHash = nullptr;
	std::string result;

	if (BCryptOpenAlgorithmProvider(&hAlg, pszAlgId, nullptr, 0) != 0) return "";

	DWORD cbHashObject = 0, cbData = 0;
	BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(cbHashObject), &cbData, 0);

	std::vector<BYTE> hashObject(cbHashObject);
	if (BCryptCreateHash(hAlg, &hHash, hashObject.data(), cbHashObject, nullptr, 0, 0) != 0)
	{
		BCryptCloseAlgorithmProvider(hAlg, 0);
		return "";
	}

	try
	{
		vmsFile file;
		file.Open(pszFile, TRUE);
		UINT64 uSize = file.get_Size();
		UINT64 trb = 0;
		BYTE buf[16384];

		for (;;)
		{
			DWORD dwRead = file.Read(buf, sizeof(buf));
			if (dwRead == 0) break;

			BCryptHashData(hHash, buf, dwRead, 0);
			trb += dwRead;

			if (m_pEvents)
			{
				double f = (double)trb / uSize * 100;
				if (!m_pEvents->OnProgressChanged(f))
				{
					trb = 0;
					break;
				}
			}
		}

		if (trb == 0 && uSize != 0)
		{
			BCryptDestroyHash(hHash);
			BCryptCloseAlgorithmProvider(hAlg, 0);
			return "";
		}

		std::vector<BYTE> digest(nDigestSize);
		BCryptFinishHash(hHash, digest.data(), nDigestSize, 0);
		result = DigestToHex(digest.data(), nDigestSize);
	}
	catch (...)
	{
	}

	BCryptDestroyHash(hHash);
	BCryptCloseAlgorithmProvider(hAlg, 0);
	return result;
}

std::string vmsHash::DigestToHex(const BYTE* pDigest, int nSize)
{
	std::string str;
	str.reserve(nSize * 2);
	for (int i = 0; i < nSize; i++)
	{
		char sz[3];
		sprintf_s(sz, sizeof(sz), "%02x", pDigest[i]);
		str += sz;
	}
	return str;
}

void vmsHash::set_EventsHandler(vmsHashEvents* pEvents)
{
	m_pEvents = pEvents;
}

void vmsHash::set_SHA2Strength(vmsHash_SHA2Strength en)
{
	switch (en)
	{
	case HSHA2S_256:
		m_nSHA2Strength = 256;
		break;
	}
}
