#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<errno.h>
#include<sys/epoll.h>
#include<sys/select.h>
#include<fcntl.h>
#include<pthread.h>
#include<map>
#include"encapsulation_mysql.h"
#include"User.h"
#include"message.pb.h"
#include"login_check.pb.h"

using namespace std;
using EncapMysql::CEncapMysql;

//define
#define	SA	                        struct sockaddr
#define	MAXLINE		        4096	/* max text line length */
#define	SERV_PORT	        5566			/* TCP and UDP client-servers */
#define	LISTENQ		        1024	/* 2nd argument to listen() */
#define     MAXEVENTS        64
//函数声明
ssize_t	 readn(int, void *, size_t);
ssize_t	 writen(int, const void *, size_t);
ssize_t	 my_read(int , char *);
ssize_t	 readline(int, void *, size_t);
int make_socket_non_blocking (int sfd);
void creatAndSetSock(int &listenfd,sockaddr_in &servaddr);
int loginCheck(int &fd,char buf[]);
void *connect_worker(void *arg);
//静态变量
static int	read_cnt;
static char	*read_ptr;
static char	read_buf[MAXLINE];
//全局变量
map<int, User> allUser;
map<int,int> fdMapID;
CEncapMysql con;
//全局常量
const char LocalIP[]="127.0.0.1";
const char DBUname[]="root";
const char DBPW[]="921029";
//结构体声明
struct cli_message
{
    int clifd;
    sockaddr_in cliaddr;
};
//函数声明

ssize_t						/* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n)
{
    size_t	nleft;
    ssize_t	nread;
    char	*ptr;

    ptr = (char *)vptr;
    nleft = n;
    while (nleft > 0)
    {
        if ( (nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
                nread = 0;		/* and call read() again */
            else
                return(-1);
        }
        else if (nread == 0)
            break;				/* EOF */

        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft);		/* return >= 0 */
}
/* end readn */

ssize_t						/* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
    size_t		nleft;
    ssize_t		nwritten;
    const char	*ptr;

    ptr = (char *)vptr;
    nleft = n;
    while (nleft > 0)
    {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;		/* and call write() again */
            else
                return(-1);			/* error */
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}
/* end writen */

ssize_t
my_read(int fd, char *ptr)
{

    if (read_cnt <= 0)
    {
again:
        if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0)
        {
            if (errno == EINTR)
                goto again;
            return(-1);
        }
        else if (read_cnt == 0)
            return(0);
        read_ptr = read_buf;
    }

    read_cnt--;
    *ptr = *read_ptr++;
    return(1);
}

ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
    size_t	n, rc;
    char	c, *ptr;

    ptr = (char *)vptr;
    for (n = 1; n < maxlen; n++)
    {
        if ( (rc = read(fd, &c,1)) == 1)
        {
            if (c == '\n')
                break;	/* skip '\n' and break */
            *(ptr++) = c;
        }
        else if (rc == 0)
        {
            *ptr = 0;
            return(n - 1);	/* EOF, n - 1 bytes were read */
        }
        else
            return(-1);		/* error, errno set by read() */
    }

    *ptr = 0;	/* null terminate like fgets() */
    return(n);
}

//设置socket为非阻塞的
int make_socket_non_blocking (int sfd)
{

    int flags, s;
    //得到文件状态标志
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl");
        return -1;
    }
    //设置文件状态标志
    flags |= O_NONBLOCK;
    //设置新的状态
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror ("fcntl");
        return -1;
    }
    return 0;
}

//初始化一个监听套接字
void creatAndSetSock(int &listenfd,sockaddr_in &servaddr)
{
    listenfd = socket(AF_INET, SOCK_STREAM, 0);//设置socket
    bzero(&servaddr, sizeof(servaddr));//初始化地址结构体为0
    servaddr.sin_family = AF_INET;//协议是ipv4
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//设置地址
    servaddr.sin_port = htons(SERV_PORT);//设置端口
    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));//绑定套接字和地址
    listen(listenfd, LISTENQ);//开启监听，但是没有调用accept来从监听到的表中提取连接
}

//增加用户到map中
void addUserToMap(int id,int cfd,int isalive,string uname)
{
                    //将查询到的用户数据放入map
                    map<int, User>::iterator iter=allUser.find(id);
                    if(iter!=allUser.end()){//已存在就要删除
                        map<int, int>::iterator iter2=fdMapID.find(iter->second.cfd);
                        fdMapID.erase(iter2);
                        allUser.erase(iter);
                    }
                    allUser.insert(map<int, User> :: value_type(id,*new User(id,uname,isalive,cfd)));
                    fdMapID.insert(map<int,int>::value_type(cfd,id));

}

//通过文件描述符清除用户信息
void eraseUserMap(int fd)
{
        map<int, int>::iterator iter=fdMapID.find(fd);
        map<int, User>::iterator iter2=allUser.find(iter->second);
        if(iter2!=allUser.end())
            allUser.erase(iter2);
        if(iter!=fdMapID.end())
             fdMapID.erase(iter->second);
}

//登录验证功能
int loginCheck(int &fd,char buf[])
{
    int n;
    //接收用户名
    memset(buf,0,MAXLINE);
    //读取用户名（注意：这里是阻塞的）
    if((n = readline(fd, buf, MAXLINE)) > 0)
    {
        //user的字段有:ID 0,Name 1,PW 2,IsAlive 3,Fd 4,LogAddr 5
        //protobuf解析
        buf[n]='\0';
        string a = buf;
        login_check lc;
        lc.ParseFromString(a);

        //连接数据库查找指定用户名的数据
        con.Connect(LocalIP,DBUname,DBPW);
        char queryString[255];
        memset(queryString,0,255);
        sprintf(queryString,"select * from MyChat.users where Name='%s'",lc.uname().c_str());
        con.SelectQuery(queryString);
        char** r = con.FetchRow();

        //如果结果集不为空
        if(r!=NULL)
        {
                if(strcmp(r[2],lc.pass().c_str())==0)//密码验证成功，把输入放入map当中
                {
                    //获取查到的数据
                    int id=atoi(r[0]),cfd=fd;
                    string uname=r[1];
                    //放入map当中
                    addUserToMap(id,cfd,1,uname);
                    //返回登录成功
                    char returnToClient[]="login ok\n";
                    writen(fd,returnToClient,sizeof(returnToClient));
                    return 0;
                }
        }
        else
        {
            cout<<"this user is not exist."<<endl;
        }
    }
    close(fd);
    return -1;
}

//通过fd得到用户id，失败返回-1
int getIDByFD(int fd)
{
    map<int, int>::iterator iter=fdMapID.find(fd);
    return  iter!=fdMapID.end() ? iter->second : -1;
}

//通过用户id得到用户文件描述符fd，失败返回-1
int getFDByID(int id)
{
    map<int, User>::iterator iter=allUser.find(id);
    return iter!=allUser.end() ? iter->second.cfd : 1;
}

//更新登录者的IP，失败返回-1
int updateLogIP(int id,sockaddr_in cliaddr){
    in_addr ipAddr=cliaddr.sin_addr;
    //***********************inet_ntoa要加互斥锁××××××××××××××××××××××××××××××××
    char *cliIP=inet_ntoa(ipAddr);//地址转换
    //********************************************************************
    char queryString[100];
    sprintf(queryString,"update MyChat.users set LogAddr='%s',Fd=%d where ID=%d",cliIP,getFDByID(id),id);
    if(con.ModifyQuery(queryString)!=0)return -1;//更新失败
    return 0;//更新成功
}

//插入聊天记录到数据库中,失败返回-1
int insertMessage(int fromUid,int gotoUid,int isAccept,const char *message)
{
    char queryString[100];
    sprintf(queryString,"insert into MyChat.messages (FromUid,GotoUid,isAccept,MessageData) values (%d,%d,%d,'%s')",fromUid,gotoUid,isAccept,message);
    if(con.ModifyQuery(queryString)!=0)return -1;//更新失败
    return 0;//更新成功
}

#define MAX_BUF_SIZE 1000
//用户登录后发送留言
void loginGetMessage(int fd,int uid)
{
    //从数据库中获取留言
    char queryString[255];
    memset(queryString,0,255);
    sprintf(queryString,"select * from MyChat.messages where GotoUid='%d' and IsAccept='0'",uid);
    con.SelectQuery(queryString);
    char** r = con.FetchRow();//怀疑有内存泄漏
    //如果结果集不为空,逐条发送给用户
    climessage cm;
    char buf[MAX_BUF_SIZE];
    string data;
    if(r!=NULL)
    {
        //设置消息对象
        cm.set_gotouid(atoi(r[2]));
        cm.set_fromuid(atoi(r[1]));
        string *tmessage=new string(r[4]);
        cm.set_allocated_sendmessage(tmessage);
        //发送消息对象
        memset(buf,0,MAX_BUF_SIZE);
        cm.SerializeToString(&data);
        sprintf(buf,"%s",data.c_str());
        writen(cm.gotouid(), buf, sizeof(buf));
        //更新数据库
        //×××××××××××××可以先不做，这样会使得每次到得到之前的留言××××××××××××××××××

        //跳到下一行数据
        r=con.FetchRow();
    }
}

#define STRING_SIZE 10  //聊天数据包的整体长度
#define TAG_ID 10           //目标用户的ID
//线程处理函数
void *connect_worker(void *arg)
{
    //从函数参数中获取数据，文件描述符和客户端地址
    int fd=(*(cli_message *)arg).clifd,n;
    sockaddr_in cliaddr=(*(cli_message *)arg).cliaddr;
    //数据缓冲区
    char buf[MAXLINE];
    //释放传递来的参数内存
    free(arg);
    //解除线程关系
    pthread_detach(pthread_self());
    //如果认证失败，停止worker线程
    if(loginCheck(fd,buf) == -1)
    {
        close (fd);
        return 0;
    }
    //更新数据库中的logaddr
    int uid=getIDByFD(fd);
    if(updateLogIP(uid,cliaddr)==-1)//如果日志记录失败，结束线程和连接
    {
        printf("update logaddr erro\n");
        //清除用户在map中的记录
        eraseUserMap(fd);
        close (fd);
        return 0;
    }
    //用户登录后发送留言信息给他
    loginGetMessage(fd,uid);
    //开始读取用户的聊天数据
    while(1)
    {
        if ( (n = readline(fd, buf, MAXLINE)) > 0)
        {
            //protobuf
            buf[n]='\0';
            //转化为string类型
            string a=buf;
            //解码为指定对象
            climessage cm;
            cm.ParseFromString(a);
            //转化为数据gotouid
            int gid=cm.gotouid();
            //int type=cm.type();
            //转化为数据message
            string climess=cm.sendmessage();
            //若用户存在发送给指定用户，转发到指定用户上
            int gfd=getFDByID(gid);
            int isSend=0;//这条信息是否已经发送
            //用户在线,转发消息
            if(gfd!=-1)
            {
                string data;
                cm.SerializeToString(&data);
                sprintf(buf,"%s",data.c_str());
                writen(gfd, buf, sizeof(buf));
                isSend=1;
            }
            //消息插入shu据库
           if(insertMessage(uid,gfd,isSend,climess.c_str())==-1)
           {
                printf("insert Message to db error\n");
                eraseUserMap(fd);
                close(fd);
                break;
           }
        }
        else if(n==0)  //客户端挥手
        {
            //清除用户在map中的记录
            eraseUserMap(fd);
            close (fd);
            break;
        }
        else//没有可读的数据
        {
            continue;
        }
    }
    return 0;
}




