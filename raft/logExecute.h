#pragma once

#include <mutex>
#include <thread>
#include "log.h"

extern std::mutex raftLock;

int executeEntry(uint32_t commandID, uint32_t lastApplied, uint32_t commitIdx, Log* log);
