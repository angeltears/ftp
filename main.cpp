#include "common.h"
#include <iostream>

int main()
{
    char s[50] = {0};
    gethostname(s, sizeof(s));
    std::cout << s <<std::endl;
    struct hostent* hp;
    hp = gethostbyname(s);
    std::cout << inet_ntoa(*(struct in_addr *)hp->h_addr);
}