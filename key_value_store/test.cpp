#include<iostream>
#include"key_value_store.h"

int main(){
    Store s;
    /*s.set("One", 1);
    s.set("Two", 2);
    s.set("Two", 3);
    s.set("Two", 4);*/
    s.set("Three", 3);
    s.set("z", 10);
    int x = s.get("Two");
    s.printStore();
    std::cout << x << std::endl;
    return 0;
}
