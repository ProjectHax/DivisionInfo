#include "PK2Reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

//-----------------------------------------------------------------------------

const char* file_seek(boost::iostreams::mapped_file & file, int64_t offset)
{
	return file.const_data() + offset;
}

int64_t file_tell(boost::iostreams::mapped_file & file, const char* offset)
{
	return offset - file.const_data();
}

int MakePathSlashWindows_1(int ch)
{
	return ch == '/' ? '\\' : ch;
}

//-----------------------------------------------------------------------------

// I use different variations of this code depending on the program, so it's not going
// to be in a common file.
std::list<std::string> TokenizeString_1(const std::string& str, const std::string& delim)
{
	// http://www.gamedev.net/community/forums/topic.asp?topic_id=381544#TokenizeString
	using namespace std;
	list<string> tokens;
	size_t p0 = 0, p1 = string::npos;
	while(p0 != string::npos)
	{
		p1 = str.find_first_of(delim, p0);
		if(p1 != p0)
		{
			string token = str.substr(p0, p1 - p0);
			tokens.push_back(token);
		}
		p0 = str.find_first_not_of(delim, p1);
	}
	return tokens;
}

//-----------------------------------------------------------------------------

void PK2Reader::Cache(std::string & base_name, PK2Entry & e)
{
	m_cache[base_name] = e;
}

//-----------------------------------------------------------------------------

PK2Reader::PK2Reader()
{
	m_root_offset = 0;
	memset(&m_header, 0, sizeof(PK2Header));
	SetDecryptionKey();
}

//-----------------------------------------------------------------------------

PK2Reader::~PK2Reader()
{
	Close();
}

//-----------------------------------------------------------------------------

size_t PK2Reader::GetCacheSize()
{
	boost::mutex::scoped_lock lock(m);
	return m_cache.size();
}

//-----------------------------------------------------------------------------

void PK2Reader::ClearCache()
{
	boost::mutex::scoped_lock lock(m);
	m_cache.clear();
}

//-----------------------------------------------------------------------------

std::string PK2Reader::GetError()
{
	boost::mutex::scoped_lock lock(m);

	std::string e = m_error.str();
	m_error.str("");
	return e;
}

//-----------------------------------------------------------------------------

void PK2Reader::SetDecryptionKey(char * ascii_key, uint8_t ascii_key_length, char * base_key, uint8_t base_key_length)
{
	boost::mutex::scoped_lock lock(m);

	if(ascii_key_length > 56)
	{
		ascii_key_length = 56;
	}

	uint8_t bf_key[56] = { 0x00 };

	uint8_t a_key[56] = { 0x00 };
	memcpy(a_key, ascii_key, ascii_key_length);

	uint8_t b_key[56] = { 0x00 };
	memcpy(b_key, base_key, base_key_length);

	for(int x = 0; x < ascii_key_length; ++x)
	{
		bf_key[x] = a_key[x] ^ b_key[x];
	}

	m_blowfish.Initialize(bf_key, ascii_key_length);
}

//-----------------------------------------------------------------------------

void PK2Reader::Close()
{
	boost::mutex::scoped_lock lock(m);

	if(file.is_open())
	{
		file.close();
	}

	m_cache.clear();
	m_root_offset = 0;
	m_error.str("");
	memset(&m_header, 0, sizeof(PK2Header));
}

//-----------------------------------------------------------------------------

bool PK2Reader::Open(std::string filename)
{
	boost::mutex::scoped_lock lock(m);

	size_t read_count = 0;

	if(file.is_open())
	{
		m_error.str(""); m_error << "There is already a PK2 opened.";
		return false;
	}

#if _WIN32
	while(filename.find("/") != std::string::npos)
		filename.replace(filename.find("/"), 1, "\\");
#endif

	try
	{
		boost::iostreams::mapped_file_params params;
		params.path = filename;
		params.flags = boost::iostreams::mapped_file_base::readonly;
		file.open(params);
	}
	catch(std::exception & e)
	{
		m_error.str(""); m_error << "Could not open the file \"" << filename << "\".\n" << e.what();
		return false;
	}

	if(!file.is_open())
	{
		m_error.str(""); m_error << "Could not open the file \"" << filename << "\".";
		return false;
	}

	const char* file_base = file.const_data();

	memcpy(&m_header, file_base, sizeof(PK2Header));
	file_base += sizeof(PK2Header);

	char name[30] = {0};
	memcpy(name, "JoyMax File Manager!\n", 21);
	if(memcmp(name, m_header.name, 30) != 0)
	{
		file.close();
		m_error.str(""); m_error << "Invalid PK2 name.";
		return false;
	}

	if(m_header.version != 0x01000002)
	{
		file.close();
		file_base = 0;
		m_error.str(""); m_error << "Invalid PK2 version.";
		return false;
	}

	m_root_offset = file_tell(file, file_base);

	if(m_header.encryption == 0)
	{
		return true;
	}

	uint8_t verify[16] = {0};
	m_blowfish.Encode("Joymax Pak File", 16, verify, 16);
	memset(verify + 3, 0, 13); // PK2s only store 1st 3 bytes

	if(memcmp(verify, m_header.verify, 16) != 0)
	{
		file.close();
		file_base = 0;
		m_error.str(""); m_error << "Invalid Blowfish key.";
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

bool PK2Reader::GetEntries(PK2Entry & parent, std::list<PK2Entry> & entries)
{
	boost::mutex::scoped_lock lock(m);

	if(!file.is_open())
	{
		m_error.str(""); m_error << "There is no PK2 loaded yet.";
		return false;
	}

	PK2EntryBlock block;
	size_t read_count = 0;

	if(parent.type != 1)
	{
		m_error.str(""); m_error << "Invalid entry type. Only folders are allowed.";
		return false;
	}

	if(parent.position < m_root_offset)
	{
		m_error.str(""); m_error << "Invalid seek index.";
		return false;
	}

	const char* file_base = file_seek(file, parent.position);

	while(true)
	{
		memcpy(&block, file_base, sizeof(PK2EntryBlock));
		file_base += sizeof(PK2EntryBlock);

		for(int x = 0; x < 20; ++x)
		{
			PK2Entry & e = block.entries[x];

			if(m_header.encryption)
			{
				m_blowfish.Decode(&e, sizeof(PK2Entry), &e, sizeof(PK2Entry));
			}

			// Protect against possible user seeking errors
			if(e.padding[0] != 0 || e.padding[1] != 0)
			{
				m_error.str(""); m_error << "The padding is not NULL. User seek error.";
				return false;
			}

			if(e.type == 1 || e.type == 2)
			{
				entries.push_back(e);
			}
		}

		if(block.entries[19].nextChain)
		{
			// More entries in the current directory
			file_base = file_seek(file, block.entries[19].nextChain);
		}
		else
		{
			// Out of the entries for the current directory
			break;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------

bool PK2Reader::GetEntry(const char * pathname, PK2Entry & entry)
{
	boost::mutex::scoped_lock lock(m);

	if(!file.is_open())
	{
		m_error.str(""); m_error << "There is no PK2 loaded yet.";
		return false;
	}

	std::string base_name = pathname;
	std::transform(base_name.begin(), base_name.end(), base_name.begin(), tolower);
	std::transform(base_name.begin(), base_name.end(), base_name.begin(), MakePathSlashWindows_1);

	std::list<std::string> tokens = TokenizeString_1(base_name, "\\");

	// Check the cache first so we can save some time on frequent accesses
	std::map<std::string, PK2Entry>::iterator itr = m_cache.find(base_name);
	if(itr != m_cache.end())
	{
		entry = itr->second;
		return true;
	}

	PK2EntryBlock block;
	size_t read_count = 0;
	std::string name;
	const char* file_base = 0;

	if(entry.position == 0)
	{
		file_base = file_seek(file, m_root_offset);
	}
	else
	{
		file_base = file_seek(file, entry.position);
	}

	while(!tokens.empty())
	{
		std::string path = tokens.front();
		tokens.pop_front();

		memcpy(&block, file_base, sizeof(PK2EntryBlock));
		file_base += sizeof(PK2EntryBlock);

		bool cycle = false;

		for(int x = 0; x < 20; ++x)
		{
			PK2Entry & e = block.entries[x];

			// I opt to decode entries as we process them rather than before hand to save 'some' processing
			// from extra entries we don't have to search.
			if(m_header.encryption)
			{
				m_blowfish.Decode(&e, sizeof(PK2Entry), &e, sizeof(PK2Entry));
			}

			// Protect against possible user seeking errors
			if(e.padding[0] != 0 || e.padding[1] != 0)
			{
				m_error.str(""); m_error << "The padding is not NULL. User seek error.";
				return false;
			}

			if(e.type == 0)
			{
				continue;
			}

			// Incurs some overhead in the long run, but the convenience it gives of not knowing exact
			// case is well worth it!
			name = e.name;
			std::transform(name.begin(), name.end(), name.begin(), tolower);

			if(name == path)
			{
				// We are at the end of the list of paths to find
				if(tokens.empty())
				{
					entry = e;
					Cache(base_name, e);
					return true;
				}
				else
				{
					// We want to make sure we only search folders, otherwise
					// bugs could result.
					if(e.type == 1)
					{
						file_base = file_seek(file, e.position);
						cycle = true;
						break;
					}

					m_error.str(""); m_error << "Invalid entry, files cannot have children!";

					// Invalid entry (files can't have children!)
					return false;
				}
			}
		}

		// We found a path entry, continue down the list
		if(cycle)
		{
			continue;
		}

		// More entries to search in the current directory
		if(block.entries[19].nextChain)
		{
			file_base = file_seek(file, block.entries[19].nextChain);
			tokens.push_front(path);
			continue;
		}

		// If we get here, what we looking for does not exist
		break;
	}

	m_error.str(""); m_error << "The entry does not exist";

	return false;
}

//-----------------------------------------------------------------------------

bool PK2Reader::ForEachEntryDo(bool (* UserFunc)(PK2Reader *, const std::string &, PK2EntryBlock &, void *), void * userdata)
{
	boost::mutex::scoped_lock lock(m);

	if(!file.is_open())
	{
		m_error.str(""); m_error << "There is no PK2 loaded yet.";
		return false;
	}

	PK2EntryBlock block;
	size_t read_count = 0;

	const char* file_base = file_seek(file, m_root_offset);

	std::list<PK2Entry> folders;
	std::list<std::string> paths;

	std::string path;

	do
	{
		if(!folders.empty())
		{
			PK2Entry e = folders.front();
			folders.pop_front();

			file_base = file_seek(file, e.position);
		}

		if(!paths.empty())
		{
			path = paths.front();
			paths.pop_front();
		}

		memcpy(&block, file_base, sizeof(PK2EntryBlock));
		file_base += sizeof(PK2EntryBlock);

		for(int x = 0; x < 20; ++x)
		{
			PK2Entry & e = block.entries[x];

			if(m_header.encryption)
			{
				m_blowfish.Decode(&e, sizeof(PK2Entry), &e, sizeof(PK2Entry));

				// Protect against possible user seeking errors
				if(e.padding[0] != 0 || e.padding[1] != 0)
				{
					m_error.str(""); m_error << "The padding is not NULL. User seek error.";
					return false;
				}
			}

			if(e.type == 1)
			{
				if(e.name[0] == '.' && (e.name[1] == 0 || (e.name[1] == '.' && e.name[2] == 0)))
				{
				}
				else
				{
					std::string cpath = path;
					if(cpath.empty())
					{
						cpath = e.name;
					}
					else
					{
						cpath += "\\";
						cpath += e.name;
					}
					folders.push_back(e);
					paths.push_back(cpath);
				}
			}
		}

		if(block.entries[19].nextChain)
		{
			int64_t pos = block.entries[19].position;
			block.entries[19].position = block.entries[19].nextChain;
			folders.push_front(block.entries[19]);
			paths.push_front(path);
			block.entries[19].position = pos;
		}

		if((*UserFunc)(this, path, block, userdata) == false)
		{
			break;
		}
	} while(!folders.empty());

	return true;
}

//-----------------------------------------------------------------------------

bool PK2Reader::ExtractToMemory(PK2Entry & entry, std::vector<uint8_t> & buffer)
{
	boost::mutex::scoped_lock lock(m);

	if(entry.type != 2)
	{
		m_error.str(""); m_error << "The entry is not a file.";
		return false;
	}
	buffer.resize(entry.size);
	if(buffer.empty())
	{
		return true;
	}

	const char* file_base = file_seek(file, entry.position);
	memcpy(&buffer[0], file_base, entry.size);

	return true;
}

const char* PK2Reader::Extract(PK2Entry & entry)
{
	return file_seek(file, entry.position);
}
//-----------------------------------------------------------------------------