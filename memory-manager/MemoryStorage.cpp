#include "MemoryStorage.h"


memoryID MemoryStorage::createIDpdu(uaecptr addr, int id) {
	memoryID pdu;
	pdu.addr = addr;
	pdu.id = id;

	return pdu;
}

uae_u32 MemoryStorage::getMemoryData(uaecptr addr, int id) {
	memoryID pdu = createIDpdu(addr, id);

	return memoryStorage[pdu];
}

void MemoryStorage::putMemoryData(uaecptr addr, int id, uae_u32 data) {

	memoryID key = *(new memoryID(createIDpdu(addr, id)));

	memoryStorage[key] = data;
	memoryStats.emplace(data, key);
}


#if 0
#include <iostream>

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

	cout << "memoryStorage: unordered_map<memoryID,uae_u32,memIDhash,memIDeqKey> database_type" << endl;
	for (database_type_iterator iter = memoryStorage.memoryStorageBeginIterator(); iter != memoryStorage.memoryStorageEndIterator(); iter++) {
		cout << "Key: [" << iter->first.id << "," << iter->first.addr << "]\t\tData: " << iter->second << endl;
	}

	cout << "memoryStats: unordered_multimap<uae_u32,memoryID> stats_type" << endl;
	for (stats_type_iterator iter = memoryStorage.memoryStatsBeginIterator(); iter != memoryStorage.memoryStatsEndIterator(); iter++) {
		cout << "Key: "<< iter->first << "\t\tData: [" << iter->second.id << "," << iter->second.addr << "]" << endl;
	}

	std::cout << "memoryStorage.size() is " << memoryStorage.memoryStorageSize() << endl;
	std::cout << "memoryStats.size() is " << memoryStorage.memoryStatsSize() << endl;
}
#endif
