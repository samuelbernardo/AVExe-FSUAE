#ifndef __MEMORY_STORAGE_H__
#define __MEMORY_STORAGE_H__

#include <unordered_map>
#include <unordered_set>
#include <openssl/sha.h>
#include <sstream>
#include <string>
#include <stdlib.h>

typedef unsigned int uae_u32;
typedef uae_u32 uaecptr;

using namespace std;

typedef struct {
	uaecptr addr;
	int id;
} memoryID;

class memIDeqKey {
public:
	bool operator() ( memoryID id1, memoryID id2) const
	{
		return id1.addr == id2.addr && id1.id == id2.id;
	}
};

inline string to_string_2args(const int& t1, const int& t2)
{
    stringstream ss;
    ss << t1;
    ss << t2;
    return ss.str();
}

class memIDhash {
public:
	size_t operator()(const memoryID& v) const
	{
	   unsigned char digest[SHA512_DIGEST_LENGTH];
	   string text = to_string_2args(v.id, v.addr);
	   char mdString[SHA512_DIGEST_LENGTH*2+1];

	   SHA512((unsigned char*) text.c_str(), text.length(), (unsigned char*)&digest);

	   for(int i = 0; i < SHA512_DIGEST_LENGTH; i++)
				sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

	   return strtoul(mdString, NULL, 0);
	}
};

// versão normal da base de dados
typedef unordered_map<memoryID,uae_u32,memIDhash,memIDeqKey> database_type;
// versão optimizada com deduplicação da base de dados
typedef unordered_set<uae_u32> deduplicated_data_type;
typedef unordered_map<memoryID,deduplicated_data_type,memIDhash,memIDeqKey> deduplicated_database_type;

typedef unordered_multimap<uae_u32,memoryID> stats_type;
typedef database_type::const_iterator database_type_iterator;
typedef stats_type::const_iterator stats_type_iterator;

class MemoryStorage {
private:
	database_type &memoryStorage = *(new database_type());
	stats_type &memoryStats = *(new stats_type());

	memoryID createIDpdu(uaecptr addr, int id);

public:
	MemoryStorage() {};

	~MemoryStorage() {
		delete &memoryStorage;
		delete &memoryStats;
	};

	/**
	 * Data structures size
	 */
	size_t memoryStorageSize() {
		return memoryStorage.size();
	}
	size_t memoryStatsSize() {
		return memoryStats.size();
	}

	/**
	 * Obter iteradores
	 */
	database_type_iterator memoryStorageBeginIterator() {
		return memoryStorage.begin();
	}
	database_type_iterator memoryStorageEndIterator() {
		return memoryStorage.end();
	}
	stats_type_iterator memoryStatsBeginIterator() {
		return memoryStats.begin();
	}
	stats_type_iterator memoryStatsEndIterator() {
		return memoryStats.end();
	}

	/**
	 * Obter dados para o endereço e id do cliente fornecidos
	 */
	uae_u32 getMemoryData(uaecptr addr, int id);

	/**
	 * guardar dados para o endereço e id fornecidos pelo cliente
	 */
	void putMemoryData(uaecptr addr, int id, uae_u32 data);

};


#endif
