#include "PK2Builder.h"
#include "shared_io.h"
#include <list>
#include <algorithm>

static int MakePathSlashWindows_1(int ch)
{
	return ch == '/' ? '\\' : ch;
}

// I use different variations of this code depending on the program, so it's not going
// to be in a common file.
static std::list<std::string> TokenizeString(const std::string& str, const std::string& delim)
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

bool PK2Builder::AddEntry(const char * pathname, PK2Entry & user_entry, void * user_data)
{
	PK2EntryBlock block;
	size_t read_count = 0;

	std::list<int64_t> roots;
	roots.push_back(m_root_offset);

	int64_t root_offset = m_root_offset;

	std::list<std::string> parts = TokenizeString(pathname, "\\/");
	parts.push_back(user_entry.name);
	if(parts.front() == "")
	{
		parts.front() = ".";
	}

	// Loop while we have folders to traverse
	while(!parts.empty())
	{
		int64_t cur_offset = roots.front();
		roots.pop_front();

		if(file_seek(m_file_header, cur_offset, SEEK_SET) != 0)
		{
			return Seek_Error_(__LINE__);
		}

		std::string current_part = parts.front();
		parts.pop_front();

		std::string current_part_case = current_part;
		std::transform(current_part_case.begin(), current_part_case.end(), current_part_case.begin(), tolower);

		// Read the block of PK2 entries
		read_count = fread(&block, 1, sizeof(PK2EntryBlock), m_file_header);
		if(read_count != sizeof(PK2EntryBlock))
		{
			Discard();
			m_error << "Could not read a PK2EntryBlock object.";
			return false;
		}

		bool reseek = false; // Do we need to reseek?

		for(int x = 0; x < 20; ++x)
		{
			PK2Entry & e = block.entries[x];

			// Skip searching null entries
			if(e.type == 0)
			{
				continue;
			}

			// Adds some overhead, but since Windows is case insensitive, we have
			// to perform this logic.
			std::string entry_name = e.name;
			std::transform(entry_name.begin(), entry_name.end(), entry_name.begin(), tolower);

			if(entry_name == current_part_case)
			{
				// When we are out of parts, the entry we want to add already exists
				if(parts.empty())
				{
					// For the builder, do not allow duplicate entries, for the writer
					// this logic would be different to allow updates.
					if(user_entry.type == 2)
					{
						m_error << "The file entry \"" << e.name << "\" already exists.";
						return false;
					}
					else
					{
						// Adding the same folder is 'ok' since nothing changes in the builder
						return true;
					}
				}
				else
				{
					if(e.type != 1)
					{
						Discard();
						m_error << "The entry path points to an invalid entry type.";
						return false;
					}

					// Save where we now need to seek
					roots.push_front(e.position);
					reseek = true;

					// The new root offset is at the current directory marker of the sub-folder
					root_offset = e.position;

					break;
				}
			}
		}

		// A path entry exists, so restart traversing from it
		if(reseek)
		{
			continue;
		}

		// If we get here, the path entry was not found, so check to see
		// if there are more nodes to search
		if(block.entries[19].nextChain)
		{
			// More entries in the current directory
			if(file_seek(m_file_header, block.entries[19].nextChain, SEEK_SET) != 0)
			{
				return Seek_Error_(__LINE__);
			}

			// Continue searching the current directory.
			// Note: We do not change root_offset since the parent directory
			// is still the same!
			parts.push_front(current_part);
			roots.push_front(block.entries[19].nextChain);

			// Restart searching
			continue;
		}

		// We can't add "parent directories" manually!
		if(current_part == "..")
		{
			roots.push_front(cur_offset);
			continue;;
		}

		// Out of the entries for the current directory,
		// add the entry path since it was not found. We
		// have to start from the current directory root
		// to fill out the file chain correctly.
		bool added = false; // Was the file added
		bool cycle = true; // Should we cycle again? (false if we created a path entry)
		cur_offset = root_offset;
		while(!added)
		{
			if(file_seek(m_file_header, cur_offset, SEEK_SET) != 0)
			{
				return Seek_Error_(__LINE__);
			}

			read_count = fread(&block, 1, sizeof(PK2EntryBlock), m_file_header);
			if(read_count != sizeof(PK2EntryBlock))
			{
				m_error << "Could not read a PK2EntryBlock object.";
				return false;
			}

			// Look for a null entry to use
			for(int x = 0; x < 20; ++x)
			{
				PK2Entry & e = block.entries[x];

				// If we have a null entry, we can use it
				if(e.type == 0)
				{
					// If we have no more parts, we can now create the user's entry
					if(parts.empty())
					{
						e.type = user_entry.type;
						e.size = user_entry.size;

						// User wants to add a file
						if(e.type == 2)
						{
							int64_t data_pos = file_tell(m_file_data);
							size_t write_count = fwrite(user_data, 1, e.size, m_file_data);
							if(write_count != e.size)
							{
								Discard();
								m_error << "Could not write the data.";
								return false;
							}
							e.position = data_pos;
						}

						// Folders are handled in the next section's logic

						// The entry was added
						added = true;

						// We need to update the current block
						cycle = false;
					}

					// Otherwise, we must create a folder along the path
					else
					{
						e.type = 1;
						e.size = 0;
					}

					int count = _snprintf_s(e.name, 80, "%s", current_part.c_str());
					memset(e.name + count, 0, (sizeof(e.name) / sizeof(e.name[0])) - count);

					if(e.type == 1)
					{
						PK2EntryBlock child_block = {0};

						if(file_seek(m_file_header, 0, SEEK_END) != 0)
						{
							return Seek_Error_(__LINE__);
						}

						// Update where the entry points to for its children
						e.position = file_tell(m_file_header);

						// Current directory
						child_block.entries[0].type = 1;
						child_block.entries[0].position = e.position;
						child_block.entries[0].name[0] = '.';

						// Parent directory
						child_block.entries[1].type = 1;
						child_block.entries[1].position = root_offset;
						child_block.entries[1].name[0] = '.';
						child_block.entries[1].name[1] = '.';

						// Save the new block
						size_t write_count = fwrite(&child_block, 1, sizeof(PK2EntryBlock), m_file_header);
						if(write_count != sizeof(PK2EntryBlock))
						{
							Discard();
							m_error << "Could not write the data.";
							return false;
						}

						// We do not need to search anymore, but we do need to update
						// the current block of PK2 entries since we modified it.
						cycle = false;

						// We will want to start traversing from the new folder now
						roots.push_front(e.position);

						// The new root offset is at the new directory entry
						root_offset = e.position;
					}

					// If we added a 20th file entry, we should write out
					// a new chain of data for the current directory.
					if(x == 19)
					{
						PK2EntryBlock sib_block = {0};

						if(file_seek(m_file_header, 0, SEEK_END) != 0)
						{
							return Seek_Error_(__LINE__);
						}

						// Update the entry's next chain for the current directory
						e.nextChain = file_tell(m_file_header);

						size_t write_count = fwrite(&sib_block, 1, sizeof(PK2EntryBlock), m_file_header);
						if(write_count != sizeof(PK2EntryBlock))
						{
							Discard();
							m_error << "Could not write the data.";
							return false;
						}

						// We need to update the current block
						cycle = false;
					}
					else
					{
						e.nextChain = 0;
					}

					break;
				}
			}

			// If we added an entry or modified the current block, write it out to disk
			if(added || cycle == false)
			{
				if(file_seek(m_file_header, cur_offset, SEEK_SET) != 0)
				{
					return Seek_Error_(__LINE__);
				}

				size_t write_count = fwrite(&block, 1, sizeof(PK2EntryBlock), m_file_header);
				if(write_count != sizeof(PK2EntryBlock))
				{
					Discard();
					m_error << "Could not write the PK2EntryBlock object.";
					return false;
				}
			}

			// If we should continue searching the current directory
			if(cycle)
			{
				// TODO: Add a new chain to jump to if the 20th entry did not
				// create a new chain already. (Should not happen with this code.)

				cur_offset = block.entries[19].nextChain;
				if(cur_offset == 0)
				{
					Discard();
					m_error << "There is not another chain to seek to.";
					return false;
				}
			}

			// Otherwise, we are done in this loop, so continue on
			else
			{
				break;
			}
		}
	}

	return true;
}

PK2Builder::PK2Builder()
{
	memset(&m_header, 0, sizeof(PK2Header));
	m_file_header = 0;
	m_file_data = 0;
	m_root_offset = 0;
}

PK2Builder::~PK2Builder()
{
	Discard();
}

std::string PK2Builder::GetError()
{
	std::string err = m_error.str();
	m_error.str("");
	return err;
}

bool PK2Builder::New(const char * name)
{
	if(m_file_header != 0 || m_file_data != 0)
	{
		m_error << "This PK2Builder is already in use. Call Finalize or Discard to close it.";
		return false;
	}

	char tmp_name[260 + 1] = {0};

	_snprintf_s(tmp_name, 260, "%s_header.pk2", name);
	m_header_name = tmp_name;
	fopen_s(&m_file_header, tmp_name, "wb+");
	if(m_file_header == NULL)
	{
		Discard();
		m_error << "Could not create the output file " << tmp_name;
		return false;
	}

	_snprintf_s(tmp_name, 260, "%s_data.pk2", name);
	m_data_name = tmp_name;
	fopen_s(&m_file_data, tmp_name, "wb+");
	if(m_file_data == NULL)
	{
		Discard();
		m_error << "Could not create the output file \"" << tmp_name << "\".";
		return false;
	}

	m_name = name;

	int count = _snprintf_s(m_header.name, 30, "%s", "JoyMax File Manager!\n");
	memset(m_header.name + count, 0, (sizeof(m_header.name) / sizeof(m_header.name[0])) - count);
	m_header.version = 0x01000002;

	size_t write_count = fwrite(&m_header, 1, sizeof(PK2Header), m_file_header);
	if(write_count != sizeof(PK2Header))
	{
		Discard();
		m_error << "Could not write the PK2Header.";
		return false;
	}

	m_root_offset = file_tell(m_file_header);

	PK2EntryBlock block = {0};
	block.entries[0].type = 1;
	block.entries[0].name[0] = '.';
	block.entries[0].position = m_root_offset;
	write_count = fwrite(&block, 1, sizeof(PK2EntryBlock), m_file_header);
	if(write_count != sizeof(PK2EntryBlock))
	{
		Discard();
		m_error << "Could not write the first PK2EntryBlock.";
		return false;
	}

	return true;
}

void PK2Builder::Discard()
{
	if(m_file_header)
	{
		fclose(m_file_header);
		m_file_header = 0;
		file_remove(m_header_name.c_str());
		m_header_name = "";
	}

	if(m_file_data)
	{
		fclose(m_file_data);
		m_file_data = 0;
		file_remove(m_data_name.c_str());
		m_data_name = "";
	}

	m_name = "";

	m_error.str("");

	memset(&m_header, 0, sizeof(PK2Header));

	m_root_offset = 0;
}

bool PK2Builder::Seek_Error_(int line)
{
	Discard();
	m_error << "[Line " << line << "] Invalid seek index.";
	return false;
}

bool PK2Builder::Finalize(char * key_ptr, size_t key_len)
{
	if(m_file_header == 0 || m_file_data == 0)
	{
		m_error << "This PK2Builder is not in use. Call New first.";
		return false;
	}

	PK2EntryBlock block;
	size_t write_count = 0;
	size_t read_count = 0;
	int64_t file_offset = 0;
	Blowfish blowfish;
	char read_buf[4096];

	if(file_seek(m_file_header, 0, SEEK_END) != 0)
	{
		return Seek_Error_(__LINE__);
	}

	file_offset = file_tell(m_file_header);

	// If the user wants to use a blowfish key
	if(key_ptr)
	{
		// Max count of 56 key bytes
		if(key_len > 56)
		{
			key_len = 56;
		}

		uint8_t bf_key[56] = { 0x00 };

		uint8_t a_key[56] = { 0x00 };
		memcpy(a_key, key_ptr, key_len);

		// This is the Silkroad base key used in all versions
		uint8_t b_key[56] = { 0x00 };
		memcpy(b_key, "\x03\xF8\xE4\x44\x88\x99\x3F\x64\xFE\x35", 10);

		// Their key modification algorithm for the final blowfish key
		for(size_t x = 0; x < key_len; ++x)
		{
			bf_key[x] = a_key[x] ^ b_key[x];
		}

		blowfish.Initialize(bf_key, key_len);
	}

	// If the use wants encryption, we have to update the header now
	if(key_ptr)
	{
		if(file_seek(m_file_header, 0, SEEK_SET) != 0)
		{
			return Seek_Error_(__LINE__);
		}

		m_header.encryption = 1;
		if(blowfish.Encode("Joymax Pak File", 16, m_header.verify, 16) == false)
		{
			Discard();
			m_error << "Blowfish Encode failed.";
			return false;
		}
		memset(m_header.verify + 3, 0, 13); // PK2s only store 1st 3 bytes

		write_count = fwrite(&m_header, 1, sizeof(PK2Header), m_file_header);
		if(write_count != sizeof(PK2Header))
		{
			Discard();
			m_error << "Could not write the updated PK2Header object.";
			return false;
		}
	}

	// Loop through and adjust the offsets of all files to the correct
	// position. In addition, if we are using encryption, fix all fields.
	std::list<int64_t> roots;
	roots.push_back(m_root_offset);
	while(!roots.empty())
	{
		int64_t offset = roots.front();
		roots.pop_front();

		if(file_seek(m_file_header, offset, SEEK_SET) != 0)
		{
			return Seek_Error_(__LINE__);
		}

		read_count = fread(&block, 1, sizeof(PK2EntryBlock), m_file_header);
		if(read_count != sizeof(PK2EntryBlock))
		{
			Discard();
			m_error << "Could not read a PK2EntryBlock object.";
			return false;
		}

		for(int x = 0; x < 20; ++x)
		{
			PK2Entry & e = block.entries[x];
			if(e.type == 0)
			{
			}
			else if(e.type == 2)
			{
				e.position += file_offset;
			}
			else if(e.type == 1)
			{
				if(e.name[0] == '.' && (e.name[1] == 0 || (e.name[1] == '.' && e.name[2] == 0)))
				{
				}
				else
				{
					roots.push_back(e.position);
				}
			}

			// If there are more entries along the chain, then we need to process those too
			if(x == 19)
			{
				if(e.nextChain)
				{
					roots.push_front(e.nextChain);
				}
			}

			// Encrypt the entry if needed
			if(key_ptr)
			{
				if(blowfish.Encode(&e, sizeof(PK2Entry), &e, sizeof(PK2Entry)) == false)
				{
					Discard();
					m_error << "Blowfish Encode failed.";
					return false;
				}
			}
		}

		// Go back to the start of the block
		if(file_seek(m_file_header, offset, SEEK_SET) != 0)
		{
			return Seek_Error_(__LINE__);
		}

		// Write the updated block now
		write_count = fwrite(&block, 1, sizeof(PK2EntryBlock), m_file_header);
		if(write_count != sizeof(PK2EntryBlock))
		{
			Discard();
			m_error << "Could not write the updated PK2EntryBlock object.";
			return false;
		}
	}

	// Now that the headers are processed, we can go through and build
	// the final PK2 file.

	std::string mfn = m_name;
	mfn += ".pk2";

	FILE * final = 0;
	fopen_s(&final, mfn.c_str(), "wb+");
	if(!final)
	{
		Discard();
		m_error << "Could not create the output file " << mfn;
		return false;
	}

	if(file_seek(m_file_header, 0, SEEK_SET) != 0)
	{
		return Seek_Error_(__LINE__);
	}

	// Write out the header data
	while(read_count = fread(read_buf, 1, 4096, m_file_header))
	{
		write_count = fwrite(read_buf, 1, read_count, final);
		if(write_count != read_count)
		{
			Discard();
			m_error << "Could not write the file data.";
			return false;
		}
	}

	if(file_seek(m_file_data, 0, SEEK_SET) != 0)
	{
		return Seek_Error_(__LINE__);
	}

	// Now write out the stored data
	while(read_count = fread(read_buf, 1, 4096, m_file_data))
	{
		write_count = fwrite(read_buf, 1, read_count, final);
		if(write_count != read_count)
		{
			Discard();
			m_error << "Could not write the file data.";
			return false;
		}
	}

	// Now we need to pad bytes to the end of the file so it remains a 
	// factor of 4096, for compatibility with Silkroad's GFXFileManager.
	file_offset = file_tell(final);
	if(file_offset % 4096 != 0)
	{
		char pad[4096] = {0};
		file_offset = (4096 - (file_offset % 4096));
		write_count = fwrite(pad, 1, static_cast<size_t>(file_offset), final);
	}

	// All done!
	fclose(final);

	// No longer need the temporary files
	Discard();

	return true;
}

bool PK2Builder::AddFolder(const char * path, const char * name)
{
	if(m_file_header == 0 || m_file_data == 0)
	{
		m_error << "This PK2Builder is not in use. Call New first.";
		return false;
	}
	PK2Entry e = {0};
	e.type = 1;
	int count = _snprintf_s(e.name, 80, "%s", name);
	memset(e.name + count, 0, (sizeof(e.name) / sizeof(e.name[0])) - count);
	return AddEntry(path, e, NULL);
}

bool PK2Builder::AddFile(const char * path, const char * name, void * data, size_t size)
{
	if(m_file_header == 0 || m_file_data == 0)
	{
		m_error << "This PK2Builder is not in use. Call New first.";
		return false;
	}
	PK2Entry e = {0};
	e.type = 2;
	e.size = size;
	int count = _snprintf_s(e.name, 80, "%s", name);
	memset(e.name + count, 0, (sizeof(e.name) / sizeof(e.name[0])) - count);
	return AddEntry(path, e, data);
}

bool PK2Builder::AddFile(const char * pathname, const char * inputname)
{
	if(m_file_header == 0 || m_file_data == 0)
	{
		m_error << "This PK2Builder is not in use. Call New first.";
		return false;
	}
	std::string path = pathname;
	std::string name = path;
	std::transform(path.begin(), path.end(), path.begin(), MakePathSlashWindows_1);
	size_t ind = path.find_last_of('\\');
	if(ind != std::string::npos)
	{
		name = name.substr(ind + 1);
		path = path.substr(0, ind);
	}
	else
	{
		path = ".";
	}
	std::vector<uint8_t> data1 = file_tovector(inputname);
	return AddFile(path.c_str(), name.c_str(), data1.empty() ? 0 : &data1[0], data1.size());
}
