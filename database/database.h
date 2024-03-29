#include<iostream>
#include<fstream>
#include<map>
#include<sstream>

class LogDatabase {
    public:
    std::map<std::string, int> map;
    std::string fileName = "store.txt";
    //Fancy way to do this is make a binary file and fix key+value sizes. So then separator doesn't matter
    //How to have any data type as value
    //
    LogDatabase(){
        std::ifstream fh;
        fh.open(fileName);
        std::string line;
        while(getline(fh, line)) {
            std::stringstream ss(line);
            std::string key, value;
            getline(ss, key, '$');
            getline(ss, value, '$');
            map[key] = stoi(value);
	    std::cout << key << " : "<< value << "\n";
        }
        fh.close();
    }
    int put(std::string key, int value);
    int get(std::string key);
    void persist();
    void printStore();
    //Probably don't need destructor

};
