#ifndef __MEMORY_STORAGE_H__
#define __MEMORY_STORAGE_H__

//#include <unordered_map>

typedef unsigned int uae_u32;
typedef uae_u32 uaecptr;

typedef struct {
	uaecptr addr;
	int id;
} memoryID;

//typedef std::unordered_map<memoryID,uae_u32> database;

class MemoryStorage {

public:
	uae_u32 getMemoryData(uaecptr addr, int id);
	void putMemoryData(uaecptr addr, int id, uae_u32 data);
};


#endif
