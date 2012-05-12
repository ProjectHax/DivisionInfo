#pragma once

#ifndef PK2READER_H_
#define PK2READER_H_

//-----------------------------------------------------------------------------

#include <stdint.h>
#include "blowfish.h"
#include <vector>
#include <map>
#include <list>
#include <sstream>
#include <string>
#include "PK2.h"

#include <boost/thread/mutex.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

//-----------------------------------------------------------------------------

class PK2Reader
{
private:

	boost::iostreams::mapped_file file;
	PK2Header m_header;
	int64_t m_root_offset;
	Blowfish m_blowfish;
	std::stringstream m_error;
	std::map<std::string, PK2Entry> m_cache;

	boost::mutex m;

private:
	PK2Reader & operator = (const PK2Reader & rhs);
	PK2Reader(const PK2Reader & rhs);
	void Cache(std::string & base_name, PK2Entry & e);

public:
	PK2Reader();
	~PK2Reader();

	// Returns how many cache entries there are. Each entry is 128 bytes of data. There
	// is also additional overhead of the string containing the full path entry name.
	size_t GetCacheSize();

	// Clears all cached entries.
	void ClearCache();

	// Returns the error if a function returns false.
	std::string GetError();

	// Sets the decryption key used for the PK2. If there is no encryption used, do not
	// call this function. Otherwise, the default variables contain keys for official SRO.
	// The other ascii_key used is "\x32\x30\x30\x39\xC4\xEA" for ZSZC and SWSRO. The
	// base_key does not change!
	void SetDecryptionKey(char * ascii_key = "169841", uint8_t ascii_key_length = 6, char * base_key = "\x03\xF8\xE4\x44\x88\x99\x3F\x64\xFE\x35", uint8_t base_key_length = 10);

	// Opens/Closes a PK2 file. There is no overhead for these functions and the file
	// remains open until Close is explicitly called or the PK2Reader object is destroyed.
	bool Open(std::string filename);
	void Close();

	// Returns true of an entry was found with the 'pathname' using 'entry' as the parent. If
	// you want to search from the root, make sure entry is a zero'ed out object.
	bool GetEntry(const char * pathname, PK2Entry & entry);

	// Returns true if a list of entries exists at the 'parent'. This will return the 
	// "current directory" of the direct child of the parent. Children of any entries
	// in this list must be 'explored' manually.
	bool GetEntries(PK2Entry & parent, std::list<PK2Entry> & entries);

	// Loops through all PK2 entries and passes them to a user function. Returns true
	// if all entries were processed or false if there was an error along the way.
	// Blocks of 20 are returned instead of individually to allow for better
	// efficiency and flexibility when implementing more complicated logic (like defragment).
	bool ForEachEntryDo(bool (* UserFunc)(PK2Reader *, const std::string &, PK2EntryBlock &, void *), void * userdata);

	// Extracts the current entry to memory. Returns true on success and false on failure.
	// Users are advised to use on common buffer to reduce the need for frequent memory 
	// reallocations on the vector side.
	bool ExtractToMemory(PK2Entry & entry, std::vector<uint8_t> & buffer);

	const char* Extract(PK2Entry & entry);
};

//-----------------------------------------------------------------------------

#endif