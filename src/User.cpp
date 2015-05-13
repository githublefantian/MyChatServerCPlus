#include "User.h"

User::User()
{
    //ctor
}

User::~User()
{
    //dtor
}
User::User(int _uid,string _uname,int _isalive,int _fd):id(_uid),name(_uname),isalive(_isalive),cfd(_fd)
{


}
