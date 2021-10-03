#include<common/myfunc.h>
void _assert(int e,char* m){
    if(!e) PANIC(m);
}