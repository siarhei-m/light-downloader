/*
  Free Download Manager Copyright (c) 2003-2014 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "FdmApp.h"
#include "vmsMetalinkFile.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

vmsMetalinkFile::vmsMetalinkFile() {}

vmsMetalinkFile::~vmsMetalinkFile() {}

BOOL vmsMetalinkFile::Parse(LPCSTR pszFile)
{
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(pszFile);
	if (!result) return FALSE;

	pugi::xml_node metalink = doc.child("metalink");
	if (!metalink) return FALSE;

	BOOL bHasOkFilesNode = FALSE;

	for (pugi::xml_node child : metalink.children())
	{
		const char* name = child.name();

		if (strcmp(name, "description") == 0)
		{
			m_strDescription = child.child_value();
		}
		else if (strcmp(name, "files") == 0)
		{
			if (ReadFilesNode(child)) bHasOkFilesNode = TRUE;
		}
	}

	return bHasOkFilesNode;
}

BOOL vmsMetalinkFile::ReadFilesNode(pugi::xml_node filesNode)
{
	BOOL bHasOkFileNode = FALSE;

	for (pugi::xml_node child : filesNode.children())
	{
		if (strcmp(child.name(), "file") == 0)
		{
			if (ReadFileNode(child)) bHasOkFileNode = TRUE;
		}
	}

	return bHasOkFileNode;
}

BOOL vmsMetalinkFile::ReadFileNode(pugi::xml_node fileNode)
{
	vmsMetalinkFile_File file;

	pugi::xml_attribute attrName = fileNode.attribute("name");
	if (attrName) file.strName = attrName.value();

	for (pugi::xml_node child : fileNode.children())
	{
		const char* name = child.name();

		if (strcmp(name, "verification") == 0)
		{
			ReadVerificationNode(child, &file);
		}
		else if (strcmp(name, "resources") == 0)
		{
			ReadResourcesNode(child, &file);
		}
		else if (strcmp(name, "os") == 0)
		{
			file.strOS = child.child_value();
		}
	}

	if (file.vMirrors.size() == 0) return FALSE;

	m_vFiles.add(file);

	return TRUE;
}

BOOL vmsMetalinkFile::ReadVerificationNode(pugi::xml_node node, vmsMetalinkFile_File* file)
{
	for (pugi::xml_node child : node.children())
	{
		if (strcmp(child.name(), "hash") == 0)
		{
			vmsMetalinkFile_File_Hash hash;
			if (ReadHashNode(child, &hash)) file->vHashes.add(hash);
		}
	}

	return TRUE;
}

BOOL vmsMetalinkFile::ReadResourcesNode(pugi::xml_node node, vmsMetalinkFile_File* file)
{
	for (pugi::xml_node child : node.children())
	{
		if (strcmp(child.name(), "url") == 0)
		{
			vmsMetalinkFile_File_Url url;
			if (ReadUrlNode(child, &url)) file->vMirrors.add(url);
		}
	}

	return TRUE;
}

BOOL vmsMetalinkFile::ReadHashNode(pugi::xml_node node, vmsMetalinkFile_File_Hash* hash)
{
	pugi::xml_attribute attrType = node.attribute("type");
	if (!attrType) return FALSE;

	hash->strAlgorithm = attrType.value();

	const char* text = node.child_value();
	if (!text || text[0] == '\0') return FALSE;

	hash->strChecksum = text;

	return TRUE;
}

BOOL vmsMetalinkFile::ReadUrlNode(pugi::xml_node node, vmsMetalinkFile_File_Url* url)
{
	pugi::xml_attribute attrType = node.attribute("type");
	if (!attrType) return FALSE;

	url->strProtocol = attrType.value();

	const char* text = node.child_value();
	if (!text || text[0] == '\0') return FALSE;

	url->strUrl = text;

	return TRUE;
}

LPCSTR vmsMetalinkFile::get_Description()
{
	return m_strDescription;
}

int vmsMetalinkFile::get_FileCount()
{
	return m_vFiles.size();
}

vmsMetalinkFile_File* vmsMetalinkFile::get_File(int nIndex)
{
	ASSERT(nIndex < get_FileCount());
	return &m_vFiles[nIndex];
}
