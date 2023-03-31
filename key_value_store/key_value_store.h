#include<iostream>
#include<fstream>
#include<map>
#include<sstream>

class Store {
    public:
    std::map<std::string, int> map;
    std::string file_name = "store.txt";
    //Fancy way to do this is make a binary file and fix key+value sizes. So then separator doesn't matter
    //How to have any data type as value
    //
    Store(){
        std::ifstream f_in;
        f_in.open(file_name);
        std::string line;
        while(getline(f_in, line)){
            std::stringstream ss(line);
            std::string key, value;
            getline(ss, key, '$');
            getline(ss, value, '$');
            map[key] = stoi(value);
        }
        f_in.close();
    }
    void set(std::string key, int value);
    int get(std::string key);
    void persist();
    void printStore();
    //Probably don't need destructor

};
