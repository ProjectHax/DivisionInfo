#include "pk2writer.h"
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>

//-----------------------------------------------------------------------------

// Start 1 byte alignment
#pragma pack(push, 1)

	// Structure to the gfx dll object, not going to try and decode it all
	struct tGFXAccess
	{
		DWORD addr1;
		DWORD addr2;
		unsigned char buffer[5930];
	};

// End 1 byte alignment
#pragma pack(pop)

//-----------------------------------------------------------------------------

// Pk2 success function
__declspec(naked) void SuccessFunction()
{
	__asm
	{
		mov eax, 1
		ret
	}
}

//-----------------------------------------------------------------------------

struct PK2WriterPIMPL
{
	// Set of registers for storing registers for context change
	DWORD dwEax_setup;
	DWORD dwEbx_setup;
	DWORD dwEcx_setup;
	DWORD dwEdx_setup;

	// Object for gfx dll access
	tGFXAccess * pGFXAccess;

	// Handle to the gfx dll
	HMODULE hDLL;

	// Holds the path to the loaded pk2 files
	char loadPk2Filename[MAX_PATH + 1];

	// Handles to the graphics functions
	FARPROC GFXDllCreateObject;
	FARPROC GFXDllReleaseObject;

	// Initialized and loaded flag
	bool bInit;
	bool bLoaded;
	
	std::stringstream err;
	std::string error;

	PK2WriterPIMPL()
	{
		// Clear our private data
		dwEax_setup = 0;
		dwEbx_setup = 0;
		dwEcx_setup = 0;
		dwEdx_setup = 0;
		bInit = false;
		bLoaded = false;
		pGFXAccess = NULL;
		hDLL = NULL;

		// Clear string memory
		memset(loadPk2Filename, 0, MAX_PATH + 1);

		// Clear function pointers
		GFXDllCreateObject = NULL;
		GFXDllReleaseObject = NULL;
	}

	// Create the pk2 api access object
	bool Initialize(const char * gfxDllFilename)
	{
		if(bInit)
		{
			err << "Initialize has already been called.";
			return false;
		}

		// Try to load the DLL
		hDLL = LoadLibraryA(gfxDllFilename);
		if(hDLL == NULL)
		{
			err << "LoadLibraryA failed for \"" << gfxDllFilename << "\". Does the file exist in the current path?";
			return false;
		}

		// Load the gfx dll startup function
		GFXDllCreateObject = GetProcAddress(hDLL, "GFXDllCreateObject");
		if(!GFXDllCreateObject)
		{
			FreeLibrary(hDLL);
			err << "GetProcAddress failed for \"GFXDllCreateObject\" in \"" << gfxDllFilename << "\".";
			return false;
		}

		// Load the gfx dll cleanup function
		GFXDllReleaseObject = GetProcAddress(hDLL, "GFXDllReleaseObject");
		if(!GFXDllReleaseObject)
		{
			FreeLibrary(hDLL);
			err << "GetProcAddress failed for \"GFXDllReleaseObject\" in \"" << gfxDllFilename << "\".";
			return false;
		}

		// Since we are doing a lot of asm code, save initial registers
		__asm pushad

		// This variable holds the pointer to the newly created access object
		DWORD dwHolder = 0;
		LPVOID pHolder = &dwHolder;

		// Store the function pointer we want to call
		FARPROC gfxFunc = GFXDllCreateObject;

		// Setup the access to the DLL by creating the object
		__asm
		{
			push 0x1007
			push pHolder
			push 0x01
			call gfxFunc
		}

		// Store the pointer to the access object
		pGFXAccess = (tGFXAccess*)(*((DWORD*)pHolder));

		// Holds the registers
		DWORD dwA = 0;
		DWORD dwB = 0;
		DWORD dwC = 0;
		DWORD dwD = 0;

		// Store this to use in asm
		LPVOID ptr1 = pGFXAccess;

		// Setup part two
		__asm
		{
			mov ecx, ptr1
			mov eax, [ecx]
			mov edx, [eax + 0xA4]
			push SuccessFunction
			call edx

			// Save these for when we set the current pk2
			mov dwA, eax
			mov dwB, ebx
			mov dwC, ecx
			mov dwD, edx
		}

		dwEax_setup = dwA;
		dwEbx_setup = dwB;
		dwEcx_setup = dwC;
		dwEdx_setup = dwD;

		// Restore original registers
		__asm popad

		// We are now initialized
		bInit = true;

		return true;
	}

	// Clean up the pk2 api access object
	bool Deinitialize()
	{
		if(bInit == false)
		{
			err << "Initialize has not yet been called.";
			return false;
		}

		if(bLoaded)
		{
			err << "A PK2 is currently open. Call Close first.";
			return false;
		}

		// If we have a gfx dll access object
		if(pGFXAccess)
		{
			// Store the function pointer we want to call
			FARPROC gfxFunc = GFXDllReleaseObject;

			// Store this to use in asm
			LPVOID ptr1 = pGFXAccess;

			// Since we are doing a lot of asm code, save initial registers
			__asm
			{
				// Save registers
				pushad

				// Free the graphics dll
				push ptr1
				call gfxFunc

				// Restore registers
				popad
			}

			// Clear the pointer
			pGFXAccess = 0;
		}

		// If the gfx dll is loaded
		if(hDLL)
		{
			// Unload the library
			FreeLibrary(hDLL);

			// Clear the pointer
			hDLL = 0;
		}

		// Clear our private data
		dwEax_setup = 0;
		dwEbx_setup = 0;
		dwEcx_setup = 0;
		dwEdx_setup = 0;
		bInit = false;
		bLoaded = false;

		// Clear string memory
		memset(loadPk2Filename, 0, MAX_PATH + 1);

		// Clear function pointers
		GFXDllCreateObject = NULL;
		GFXDllReleaseObject = NULL;

		return true;
	}

	// Closes the open PK2 file
	bool Close()
	{
		if(bInit == false)
		{
			err << "Initialize has not yet been called.";
			return false;
		}

		if(!bLoaded)
		{
			err << "Open has not yet been called.";
			return false;
		}

		// Store this to use in asm
		LPVOID ptr1 = pGFXAccess;

		// Start opening the file
		DWORD var1 = 0;
		LPVOID lpVar1 = &var1;

		// Activate the pk2 file to operate on
		__asm
		{
			// Save registers
			pushad

			// Restore these from after the gfx dll was created
			mov eax, dwEax_setup
			mov ebx, dwEbx_setup
			mov ecx, dwEcx_setup
			mov edx, dwEdx_setup

			// Params to open the pk2 for access
			push lpVar1
			push 0
			push 0

			// Code to open the pk2 for access
			mov ecx, ptr1
			mov edx, [ecx]
			mov eax, [edx + 0x10]		
			call eax

			// Restore registers
			popad
		}

		// No longer loaded
		bLoaded = false;

		return true;
	}

	// Sets a pk2 file to import into
	bool Open(const char * pk2Filename, void * accessKey, unsigned char accessKeyLen)
	{
		if(bInit == false)
		{
			err << "Initialize has not yet been called.";
			return false;
		}

		if(bLoaded)
		{
			err << "A PK2 is currently open. Call Close first.";
			return false;
		}

		if(accessKeyLen == 0 || accessKeyLen > 56)
		{
			err << "The key length may only be 1 - 56 bytes long.";
			return false;
		}

		// Result of a function
		DWORD dwResult = 0;

		// Start opening the file
		DWORD var1 = accessKeyLen;
		LPVOID lpVar1 = &var1;

		// Store these as explicit pointers
		char * paccess = (char *)accessKey;
		char * pfilename = loadPk2Filename;

		// Store the values
		_snprintf_s(loadPk2Filename, MAX_PATH, "%s", pk2Filename);

		// Store this to use in asm
		LPVOID ptr1 = pGFXAccess;

		// Activate the pk2 file to operate on
		__asm
		{
			// Save registers
			pushad

			// Restore these from after the gfx dll was created
			mov eax, dwEax_setup
			mov ebx, dwEbx_setup
			mov ecx, dwEcx_setup
			mov edx, dwEdx_setup

			// Params to open the pk2 for access
			push lpVar1
			push paccess
			push pfilename

			// Code to open the pk2 for access
			mov ecx, ptr1
			mov edx, [ecx]
			mov eax, [edx + 0x10]		
			call eax

			// Store the result
			mov dwResult, eax

			// Restore registers
			popad
		}

		// Check the result
		if(dwResult == 0)
		{
			err << "There was a problem correctly accessing the GFXFileManager DLL.";
			return false;
		}

		// We have a file loaded now
		bLoaded = true;

		return true;
	}

	// Replace a file in a pk2 with another from file
	bool ImportFile(const char * entryFilename, void * fileBuffer, int fileSize)
	{
		if(bInit == false)
		{
			err << "Initialize has not yet been called.";
			return false;
		}

		if(!bLoaded)
		{
			err << "Open has not yet been called.";
			return false;
		}

		// Pointer to the input data
		LPBYTE updateData = (LPBYTE)fileBuffer;

		// Size of the new data
		DWORD updateDataSize = fileSize;

		// Store the pointer to the string for asm access
		char * pkFilePathName = (char*)entryFilename;

		// Make sure there is data to replace the entry with
		/*if(updateDataSize == 0)
		{
			err << "The replacement file must be at least 1 byte in size.";
			return false;
		}*/

		// Result of the operation
		DWORD result = 0;

		// Store this to use in asm
		LPVOID ptr1 = pGFXAccess;

		// Step 1
		__asm
		{
			// Setup dll access functions
			mov esi, ptr1
			mov edx, [esi]
			mov eax, [edx + 0x3C]
			mov ecx, esi

			// Prepare the pk2 to be patched
			push updateDataSize
			push pkFilePathName
			call eax

			// Save the result, which is the file in the update process
			mov result, eax
			mov ebx, eax

			// Save regs
			pushad
		}

		// Check result
		if(result == 0)
		{
			err << "Step 1 Failed.";
			return false;
		}

		// How many bytes were written
		DWORD asmWrote = 0;

		// Step 2
		__asm
		{
			popad 

			// Store how many bytes were written
			lea ecx, asmWrote
			push ecx

			// Save the file size
			mov eax, updateDataSize
			push eax

			// Prepare the dll functions
			mov edi, [esi]
			add edi, 0x4C
			mov edx, [edi]

			// Data to update
			mov eax, updateData
			push eax

			// File # to patch
			push ebx

			mov ecx, esi
			call edx
			mov result, eax

			// Save regs
			pushad
		}

		// Check result
		if(result == 0)
		{
			err << "Step 2 Failed.";
			return false;
		}

		// Step 3
		__asm
		{
			popad
			mov eax, [esi]
			mov edx,[eax + 0x44]
			push ebx
			mov ecx, esi
			call edx
			mov result, eax
		}

		// Check result
		if(result == 0)
		{
			err << "Step 3 Failed.";
			return false;
		}

		return true;
	}

	// Replace a file in a pk2 with another from file
	bool ImportFile(const char * entryFilename, const char * inputFilename)
	{
		if(bInit == false)
		{
			err << "Initialize has not yet been called.";
			return false;
		}

		if(!bLoaded)
		{
			err << "Open has not yet been called.";
			return false;
		}

		// Pointer to the input data
		LPBYTE updateData = 0;

		// Size of the new data
		DWORD updateDataSize = 0;

		// Store the pointer to the string for asm access
		char * pkFilePathName = (char*)entryFilename;

		// Handle to the input file
		HANDLE hFile = 0;

		// Try and open the settings config file to verify that it is there
		hFile = CreateFileA(inputFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if(hFile == INVALID_HANDLE_VALUE)
		{
			err << "Could not open the input file: \"" << inputFilename << "\".";
			return false;
		}

		// Store file size
		updateDataSize = GetFileSize(hFile, NULL);

		// Make sure there is data to replace the entry with
		/*if(updateDataSize == 0)
		{
			// Close the handle
			CloseHandle(hFile);

			err << "The replacement file must be at least 1 byte in size.";
			return false;
		}*/

		// Vector of data to save us on memory management
		std::vector<unsigned char> data(updateDataSize, 0);

		if(updateDataSize)
		{
			// Allocate data for the new file
			updateData = &data[0];

			// Read in the data
			DWORD dwRead = 0;
			if(ReadFile(hFile, updateData, updateDataSize, &dwRead, 0) == 0)
			{
				// Close the handle
				CloseHandle(hFile);

				err << "Could not read from the input file.";
				return false;
			}

			// Make sure we read in all the data
			if(dwRead != updateDataSize)
			{
				// Close the handle
				CloseHandle(hFile);

				err << "Could not read all of the data from the input file.";
				return false;
			}
		}

		// Close the handle
		CloseHandle(hFile);

		return ImportFile(entryFilename, updateData, updateDataSize);
	}

	const char * GetError()
	{
		error = err.str();
		err.str("");
		return error.c_str();
	}
};

//-----------------------------------------------------------------------------

// Ctor
PK2Writer::PK2Writer()
{
	m_PK2WriterPIMPL = new PK2WriterPIMPL;
}

//-----------------------------------------------------------------------------

// Dtor
PK2Writer::~PK2Writer()
{
	m_PK2WriterPIMPL->Close();
	m_PK2WriterPIMPL->Deinitialize();
	delete m_PK2WriterPIMPL;
}

//-----------------------------------------------------------------------------

bool PK2Writer::Initialize(const char * gfxDllFilename)
{
	return m_PK2WriterPIMPL->Initialize(gfxDllFilename);
}

//-----------------------------------------------------------------------------

bool PK2Writer::Deinitialize()
{
	return m_PK2WriterPIMPL->Deinitialize();
}

//-----------------------------------------------------------------------------

bool PK2Writer::Open(const char * pk2Filename, void * accessKey, unsigned char accessKeyLen)
{
	return m_PK2WriterPIMPL->Open(pk2Filename, accessKey, accessKeyLen);
}

//-----------------------------------------------------------------------------

bool PK2Writer::Close()
{
	return m_PK2WriterPIMPL->Close();
}

//-----------------------------------------------------------------------------

bool PK2Writer::ImportFile(const char * entryFilename, const char * inputFilename)
{
	return m_PK2WriterPIMPL->ImportFile(entryFilename, inputFilename);
}

//-----------------------------------------------------------------------------

bool PK2Writer::ImportFile(const char * entryFilename, void * fileBuffer, int fileSize)
{
	return m_PK2WriterPIMPL->ImportFile(entryFilename, fileBuffer, fileSize);
}

//-----------------------------------------------------------------------------

const char * PK2Writer::GetError()
{
	return m_PK2WriterPIMPL->GetError();
}

//-----------------------------------------------------------------------------
