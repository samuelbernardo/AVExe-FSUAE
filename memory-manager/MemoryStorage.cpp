#include "MemoryStorage.h"


memoryID MemoryStorage::createIDpdu(uaecptr addr, int id) {
	memoryID pdu;
	pdu.addr = addr;
	pdu.id = id;

	return pdu;
}

#ifdef __MEMORY_DEDUPLICATION__
uae_u32 MemoryStorage::getMemoryData(uaecptr addr, int id) {
	memoryID key = createIDpdu(addr, id);

	return *dedupMemoryStorage[key];
}

void MemoryStorage::putMemoryData(uaecptr addr, int id, uae_u32 data) {

	memoryID key = *(new memoryID(createIDpdu(addr, id)));

	dedupMemoryStorage[key] = &dedupDataStorage.emplace(data, data).first->second;

	memoryStats.emplace(data, key);
}
#else
uae_u32 MemoryStorage::getMemoryData(uaecptr addr, int id) {
	memoryID key = createIDpdu(addr, id);

	return memoryStorage[key];
}

void MemoryStorage::putMemoryData(uaecptr addr, int id, uae_u32 data) {

	memoryID key = *(new memoryID(createIDpdu(addr, id)));

	memoryStorage[key] = data;
	memoryStats.emplace(data, key);
}
#endif


#if 0
#include <iostream>
#include <assert.h>
#include <fstream>

static MemoryStorage &memoryStorage = *(new MemoryStorage());

typedef struct {
	int op;
	int id;
	uaecptr addr;
	uae_u32 data;
} memPDU;

int main (int argc, char* argv[])
{
	memPDU memoryBank;
	memoryBank.addr = 1211331;
	memoryBank.data = 1233241;
	memoryBank.id = 1;
	int data = 0;

	memoryStorage.putMemoryData(memoryBank.addr, memoryBank.id, memoryBank.data);
	memoryStorage.putMemoryData(21421341, 1, 1523513);
	memoryStorage.putMemoryData(memoryBank.addr, 2, 3421234);
	memoryStorage.putMemoryData(21421341, 2, 1523513);
	memoryStorage.putMemoryData(memoryBank.addr, memoryBank.id, 3421234);
	memoryStorage.putMemoryData(memoryBank.addr, memoryBank.id, memoryBank.data);
	memoryStorage.putMemoryData(21421341, 1, 1523513);
	memoryStorage.putMemoryData(memoryBank.addr, 2, 3421234);
	memoryStorage.putMemoryData(21421341, 2, 1523513);
	memoryStorage.putMemoryData(memoryBank.addr, memoryBank.id, 3421234);

	data = memoryStorage.getMemoryData(memoryBank.addr, memoryBank.id);

	cout << "Data variable test: " << data << endl;

	/**
	 * Apresentação de resultados
	 */
#ifdef __MEMORY_DEDUPLICATION__
	int memSize = memoryStorage.dedupMemoryStorageSize();
	int dataSize = memoryStorage.dedupDataStorageSize();
#else
	int memSize = memoryStorage.memoryStorageSize();
#endif
	int allRequests = memoryStorage.memoryStatsSize();
	assert(allRequests!=0);
	float access_ratio = ((float)memSize)/((float)allRequests);

	ofstream logFile;
	logFile.open("log.txt");
#ifdef __MEMORY_DEDUPLICATION__
	logFile << "dedupMemoryStorage: unordered_map<memoryID,uae_u32*,memIDhash,memIDeqKey> deduplicated_database_type" << endl;
	for (deduplicated_database_iterator iter = memoryStorage.dedupMemoryStorageBeginIterator(); iter != memoryStorage.dedupMemoryStorageEndIterator(); iter++) {
		logFile << "Key: [" << iter->first.id << "," << iter->first.addr << "]\t\tData: ";
		logFile << *(iter->second);
		logFile << endl;
	}
#else
	logFile << "memoryStorage: unordered_map<memoryID,uae_u32,memIDhash,memIDeqKey> database_type" << endl;
	for (database_type_iterator iter = memoryStorage.memoryStorageBeginIterator(); iter != memoryStorage.memoryStorageEndIterator(); iter++) {
		logFile << "Key: [" << iter->first.id << "," << iter->first.addr << "]\t\tData: " << iter->second << endl;
	}
#endif
	logFile << endl;
	logFile << endl;
	logFile << "memoryStats: unordered_multimap<uae_u32,memoryID> stats_type" << endl;
	for (stats_type_iterator iter = memoryStorage.memoryStatsBeginIterator(); iter != memoryStorage.memoryStatsEndIterator(); iter++) {
		logFile << "Key: "<< iter->first << "\t\tData: [" << iter->second.id << "," << iter->second.addr << "]" << endl;
	}

#ifdef __MEMORY_DEDUPLICATION__
	assert(memSize!=0);
	float optimization_ratio = ((float)dataSize)/((float)memSize);
	cout << "dedupMemoryStorage.size() is " << memSize << endl;
	cout << "dedupDataStorage.size() is " << dataSize << endl;
	cout << "dedupMemoryStats.size() is " << allRequests << endl;
	cout << "Optimization acquired in run was: " << optimization_ratio << endl;
#else
	cout << "memoryStorage.size() is " << memSize << endl;
	cout << "memoryStats.size() is " << allRequests << endl;
#endif
	cout << "Memory access ration was: " << access_ratio << endl;

}
#endif
