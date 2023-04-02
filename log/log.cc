#include"log.h"

int main(){
   LogEntry entry[4];
   Log log;

   entry[0].GetOrPut = 1;
   entry[2].GetOrPut = 1;

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
