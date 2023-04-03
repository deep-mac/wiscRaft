#include <limits.h>
#include "logExecute.h"

std::mutex raftLock;

int executeEntry(uint32_t commandTerm, uint32_t commandID, uint32_t lastApplied, uint32_t commitIdx, Log* log, LogDatabase *database) {

    int retValue = INT_MAX;
    while (1) {
	raftLock.lock();
	uint32_t logEntryIdx = commitIdx - lastApplied - 1;
        LogEntry head_entry = log->get_head();
//	if ((logEntryIdx >=0) && (commandID == log->get_entry(logEntryIdx)->commandID) && (commandTerm == log->get_entry(logEntryIdx)->command_term)) {
	if ((logEntryIdx >=0) && (commandID == head_entry.commandID) && (commandTerm == head_entry.command_term)) {

		assert(head_entry.commandID == commandID;
		assert(head_entry.command_term == commandTerm);
	        if (head_entry.GetOrPut) {
	            ret = database->get(head_entry.key);
	        } else {
	            ret = database->put(head_entry.key, head_entry.value);
	        }

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
