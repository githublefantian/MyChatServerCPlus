#include<string>
using namespace std;
#ifndef USER_H
#define USER_H
class User
{
    public:
        User();
        User(int _uid,string _uname,int _isalive,int _fd);
        virtual ~User();
        int id;
        string name;
        int isalive;
        int cfd;
    protected:
    private:
};

#endif // USER_H
