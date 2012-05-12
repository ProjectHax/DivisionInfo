#pragma once

#ifndef PK2BUILDER_H_
#define PK2BUILDER_H_

//-----------------------------------------------------------------------------

#include <stdint.h>
#include "blowfish.h"
#include "PK2.h"
#include <sstream>
#include <string>

//-----------------------------------------------------------------------------

class PK2Builder
{
private:
	PK2Header m_header;
	int64_t m_root_offset;
	FILE * m_file_header;
	FILE * m_file_data;
	std::string m_name;
	std::string m_header_name;
	std::string m_data_name;
	std::stringstream m_error;

private:
	PK2Builder & operator = (const PK2Builder & rhs);
	PK2Builder(const PK2Builder & rhs);

	bool AddEntry(const char * pathname, PK2Entry & user_entry, void * user_data);
	bool Seek_Error_(int line);

public:
	PK2Builder();
	~PK2Builder();

	// Returns the error if a function fails.
	std::string GetError();

	// Creates a new PK2 for building, only pass the title of the PK2 and not
	// the extension!
	bool New(const char * name);

	// Discards the current temporary files for the PK2 being built.
	void Discard();

	// Creates the output file using the specified key.
	bool Finalize(char * key_ptr = "169841", size_t key_len = 6);

	// Adds a new folder (adds all paths as needed).
	bool AddFolder(const char * path, const char * name);

	// Adds a new file based on existing data (adds all paths as needed).
	bool AddFile(const char * path, const char * name, void * data, size_t size);

	// Adds a new file based on an existing physical file (adds all paths as needed).
	bool AddFile(const char * pathname, const char * inputname);
};

//-----------------------------------------------------------------------------

#endif
