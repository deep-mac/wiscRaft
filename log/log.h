#include<iostream>
#include<deque>
#include<string>
#include<fstream>
#include<mutex>
#include "database.h"
#include <limits.h>
#include <cassert>

class LogEntry{
 public:

 bool GetOrPut;
 std::string key;
 int value;
 uint32_t commandID;

 LogEntry() : GetOrPut(true),
	      key(""),
	      value(0),
	      commandID(INT_MAX) {}

 LogEntry(const LogEntry& copy) {
  GetOrPut = copy.GetOrPut;
  key = copy.key;
  value = copy.value;
  commandID = copy.commandID;
 }

 void setEntry(bool command, std::string newKey, int newValue, uint32_t newID);
};

class Log {

 std::deque<LogEntry> log;
 std::fstream fh;
 uint32_t commitID;
 LogDatabase database;
 public:

 Log() : database() {
    commitID = 0;
 }

 //Should be called atomically for each append
 void LogAppend(LogEntry& Entry);
 void LogAppend(bool command, std::string newKey, int newValue, uint32_t newID);

 //Should be called atomically once for every execute
 void LogCleanup();

 //Used to get the head for execution
 LogEntry get_head();

 void commitEntry(uint32_t commandID);

 int executeEntry();

 //Needed for debug purposes
 void print();
};
