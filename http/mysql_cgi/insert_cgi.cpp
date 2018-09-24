#include "comm.h"
using namespace std;

void version()
{
    std::cout<<"Client Version:"<<mysql_get_client_info()<<std::endl;
}

int main()
{

    char arg[1024];
    if(getenv("METHOD")){
        if(strcmp("GET",getenv("METHOD"))==0){
            //GET
            strcpy(arg,getenv("QUERY_STRING"));
        }else{
            //POST
            int content_length=atoi(getenv("CONNECT_LENGTH"));
            int i=0;
            char c;
            for(;i<content_length;i++){
                read(0,&c,1);
                arg[i]=c;
            }
            arg[i]='\0';
        }
    }
    std::cout << arg << std::endl;
    strtok(arg,"&=");
    std::string name=strtok(NULL,"&=");
    strtok(NULL,"&=");
    std::string school=strtok(NULL,"&=");
    strtok(arg,"&=");
    std::string hobby=strtok(NULL,"&=");

    MYSQL *myfd=initMysql();
    connectMysql(myfd);
    insertMysql(myfd);
    insertMysql(myfd,name,school,hobby);
    connectClose(myfd);
    return 0;
    
}

