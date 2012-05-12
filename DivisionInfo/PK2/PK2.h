#pragma once

#ifndef PK2_H_
#define PK2_H_

//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------

#pragma pack(push, 1)

struct PK2Header
{
	char name[30]; // PK2 internal name
	uint32_t version; // PK2 version
	uint8_t encryption; // does the PK2 have encryption?
	uint8_t verify[16]; // used to test the blowfish key
	uint8_t reserved[205]; // unused
};

struct PK2Entry
{
	uint8_t type; // files are 2, folders are 1, null entries are 0
	char name[81]; // entry name
	uint64_t accessTime; // Windows time format
	uint64_t createTime; // Windows time format
	uint64_t modifyTime; // Windows time format
	int64_t position; // position of data for files, position of children for folders
	uint32_t size; // size of files
	int64_t nextChain; // next chain in the current directory
	uint8_t padding[2]; // So blowfish can be used directly on the structure
};

struct PK2EntryBlock
{
	PK2Entry entries[20]; // each block is 20 contiguous entries
};

#pragma pack(pop)

//-----------------------------------------------------------------------------

#endif
