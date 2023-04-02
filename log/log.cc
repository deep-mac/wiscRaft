#include"log.hh"

//Should be called atomically for each append
void Log::LogAppend(LogEntry& Entry){
  logLock.lock();
  log.push_back(Entry);
  //fh.open("log.txt",std::fstream::app);
  std::string line = std::to_string(Entry.isRead) + "$" + Entry.key + "$" + std::to_string(Entry.value) + "$" + std::to_string(Entry.commandID);
  fh << line << std::endl;
  //fh.close();
  logLock.unlock();
}

//Should be called atomically once for every execute
void Log::LogCleanup(){
  logLock.lock();
  log.pop_front();
  fh.open("log.txt",std::fstream::trunc);
  for(LogEntry val : log){
   std::string line = std::to_string(val.isRead) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.commandID);
   fh<<line<<std::endl;
  }
  fh.close();
  logLock.unlock();
}

//Used to get the head for execution
LogEntry Log::get_head(){
  return log.front(); 
}

//Needed for debug purposes
void Log::print(){
 for(LogEntry val:log){
  std::string line = std::to_string(val.isRead) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.commandID);
  std::cout<<line<<std::endl;
 }
}

void Log::commitEntry(uint32_t commandID) {
    logLock.lock();
    assert(commitID < log.size());
    if (commandID != log[commitID].commandID) {
    // Shouldn't be here. The commandID passed should always be the same as that of the log entry waiting to commit
       std::cout << __FILE__ << ":" << __LINE__ << ". Incorrect commandID received! Returing without doing anything" << std::endl;
       return;
    }
    commitID++;
    logLock.unlock();
}

int Log::executeEntry() {
    int ret = 0;
    assert(commitID > 0);
    LogEntry entry = get_head();
    if (entry.isRead) {
	ret = database.get(entry.key);
    } else {
    	database.put(entry.key, entry.value);
    }
    commitID--;
    return ret;
}

int main(){
   LogEntry entry[4];
   Log log;

   entry[0].isRead = false;
   entry[2].isRead = false;

   for(int i=0; i<4; i++)
    log.LogAppend(entry[i]);

   //Do Execution here, do not pop.
   LogEntry exec = log.get_head();
   //Execute the entry based on conditions
   log.print();

   for(int i=0; i<4; i++)
    log.LogCleanup();

   log.print();

   return 0;
}
