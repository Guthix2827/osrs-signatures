#include "domain.hpp"
#include <iostream>
#include <cstdlib>
using namespace signature;
void check(bool value) {if(!value){std::cerr<<"Check failed\n";std::exit(1);}}
int main(){
    check(combat(1,1,1,10,1,1,1)==3);
    check(combat(99,99,99,99,99,99,99)==126);
    check(combat(1,1,1,10,99,1,1)==50);
    check(combat(1,1,1,10,1,1,99)==50);
    check(combat(1,1,1,10,99,1,1)==combat(1,1,1,10,97,1,1)+1);
    check(normalizeName("  Zezima  ")=="Zezima");
    check(keyName("Test_Player")==keyName("test player"));
    for(auto n:{"","../../passwd","a?b","1234567890123"}) {
        bool rejected=false;try{normalizeName(n);}catch(...){rejected=true;}check(rejected);
    }
    bool rejected=false;try{combat(0,1,1,10,1,1,1);}catch(...){rejected=true;}check(rejected);
    check(validId("0123456789abcdef0123456789abcdef"));check(!validId("../../etc/passwd"));
    std::cout<<"Domain checks passed\n";
}
