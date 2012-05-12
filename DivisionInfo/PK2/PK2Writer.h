#pragma once

#ifndef PK2WRITER_H_
#define PK2WRITER_H_

//-----------------------------------------------------------------------------

class PK2Writer
{
	friend struct PK2WriterPIMPL;
	PK2WriterPIMPL * m_PK2WriterPIMPL;

public:
	PK2Writer();
	~PK2Writer();

	// Sets up GfxFileManager.DLL for PK2 operations. This function must be
	// called first.
	bool Initialize(const char * gfxDllFilename);

	// Cleans up GfxFileManager.DLL. This function must be called before the
	// program exits and after Close if a PK2 file was opened.
	bool Deinitialize();

	// Opens a PK2 file for writing. Use:
	//		"169841" - For official sro, mysro
	//		"\x32\x30\x30\x39\xC4\xEA" - for zszc, swsro
	// Refer to this guide:
	// http://www.elitepvpers.de/forum/sro-guides-templates/612789-guide-finding-pk2-blowfish-key-5-easy-steps.html
	// To get the "base key"
	bool Open(const char * pk2Filename, void * accessKey, unsigned char accessKeyLen);

	// Closes an opened PK2 file. This function must be called before the program
	// exits and before Deinitialize is called.
	bool Close();

	// Imports a file to the PK2. 'entryFilename' should be the full path the
	// file should have in the PK2.
	bool ImportFile(const char * entryFilename, const char * inputFilename);

	// Imports a file's buffer to the PK2. 'entryFilename' should be the full path the
	// file should have in the PK2.
	bool ImportFile(const char * entryFilename, void * fileBuffer, int fileSize);

	// Returns the last error if one of the above functions fails.
	const char * GetError();
};

//-----------------------------------------------------------------------------

#endif
