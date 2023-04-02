#include "logExecute.h"

std::mutex raftLock;

int executeEntry(uint32_t commandID, uint32_t lastApplied, uint32_t commitIdx, Log* log) {

    int retValue = INT_MAX;
    while (1) {
	raftLock.lock();
	uint32_t logEntryIdx = commitIdx - lastApplied - 1;
	if ((logEntryIdx >=0) && (commandID == log->getEntry(logEntryIdx)->commandID)) {
		retValue = log->executeEntry();
		log->LogCleanup();
		raftLock.unlock();
		break;
	} else {
	   raftLock.unlock();
	   std::this_thread::yield();
	}
    }

    return retValue;
}
