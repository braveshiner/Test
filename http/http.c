
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<signal.h>
//./
#define MAX 1024

static void usage(const char *proc)

{
	printf("Usage:%s port\n",proc);
}
static void show_404(int sock)
{
	char line[MAX];
	sprintf(line,"HTTP/1.0  404 Not Found\r\n");
	send(sock,line,strlen(line),0);
	sprintf(line,"Content_Type:text/html\r\n");
	send(sock,line,strlen(line),0);
	sprintf(line,"\r\n");
	send(sock,line,strlen(line),0);

	struct stat st;
	stat("wwwroot/404.html",&st);
	int fd=open("wwwroot/404.html",O_RDONLY);
	sendfile(sock,fd,NULL,st.st_size);
	close(fd);
}
static int getLine(int sock,char line[],int num)
{
	char c='x';
	int i=0;
	while(c!='\n'&&i<num-1){
		ssize_t s=recv(sock,&c,1,0);
		if(s>0){
			if(c=='\r'){
				recv(sock,&c,1,MSG_PEEK);
				if(c=='\n'){
					recv(sock,&c,1,0);
				}else{
					c='\n';
				}
			}
			line[i++]=c; 
		} else{
			break;
		}
	}
	line[i]=0;
	return i;
}

void clearHeader(int sock)
{
	char line[MAX];
	do{
		getLine(sock,line,MAX);
	}while(strcmp(line,"\n")!=0);
}
//出现异常 报头全部丢弃
void echoError(int sock,int status_code)
{
	switch(status_code){
		case 404:
			show_404(sock);
			break;
		case 500:
			break;
		case 400:
			break;
		case 403:
			break;
defalt:
			break;
	}
}
//错误处理
int echo_www(int sock,char *path,int size)
	//空行确定报与报的边界  解决粘包问题
{
	clearHeader(sock);
	//清报头
	int fd=open(path,O_RDONLY);
	if(fd<0)
		//打开失败
	{
		return 500;

	}
	char *stuff=path+strlen(path)-1;
	while(*stuff!='.'&&stuff!=path){
		stuff--;
	}
	//后缀
	char line[MAX];
	sprintf(line,"HTTP/1.0 200 OK\r\n");
	//状态行  状态码 tcp 有发送缓冲区
	send(sock,line,strlen(line),0);
	//不能sizeof \r\n后面垃圾数据

	if(strcmp(stuff,".html")==0){
		sprintf(line,"Content-Type:text/html\r\n");
	}
	else if(strcmp(stuff,".css")==0){
		sprintf(line,"Content-Type:text/css\r\n");
	}
	if(strcmp(stuff,".js")==0){
		sprintf(line,"Content-Type:application/x-javascripty\r\n");
	}
	send(sock,line,strlen(line),0);

	sprintf(line,"Content-Length:%d\r\n",size);
	send(sock,line,strlen(line),0);

	sprintf(line,"\r\n");
	send(sock,line,strlen(line),0);

	sendfile(sock,fd,NULL,size);
	close(fd);
	return 200;
}
int exe_cgi(int sock,char *method,char *path,char *query_string)
{
	//arg,exe 处理参数  执行程序
	char line[MAX];
	char method_env[MAX/16];
	char query_string_env[MAX];
	char content_length_env[MAX/16];

	int content_length=-1;
	if(strcasecmp(method,"GET")==0){
		clearHeader(sock);
	}
	else{//post
		do{
			getLine(sock,line,MAX);
			if(strncasecmp(line,"Content-length:",16)==0)
				//strncasecmp指定子长
			{
				content_length=atoi(line+16);
				//content- length：后面内容
			}
		}while(strcmp(line,"\n")!=0);
		if(content_length==-1){
			return 400;
		}

	}
	//在线程 子进程数据交给浏览器  父有socket 只有父进程可以和浏览器打交道
	//进程通信用管道
	int input[2];
	int output[2];
	//子进程角度
	pipe(input);
	pipe(output);

	pid_t id=fork();
	if(id<0){
		return 500;
	}
	else if(id==0){//child  0 读 1写 （笔）
		close(input[1]);
		close(output[0]);

		dup2(input[0],0);
		dup2(output[1],1);
		//重定向，从标准输入改成 input[0]
		//通过环境变量传参  导环境变量
		sprintf(method_env,"METHOD=%s",method);
		putenv(method_env);
		if(strcasecmp(method,"GET")==0){
			sprintf(query_string_env,"QUERY_STRING=%s",query_string);
			putenv(query_string_env);
		}
		else{//post
			sprintf(content_length_env,"CONTENT_LENGTH=%d",content_length);
			putenv(content_length_env);
		}
		execl(path,path,NULL);
		//环境变量不能替换
		//带有绝对路径可以执行 /bin/ls -al
		close(input[0]);
		close(output[1]);
		exit(1);
	}   
	else{//father
		close(input[0]);
		close(input[1]);
		int i=0;
		char c;
		if(strcasecmp(method,"POST")==0){
			for(;i<content_length;i++){
				recv(sock,&c,1,0);
				//从sock读
				write(input[1],&c,1);
			}
		}
		sprintf(line,"HTTP/1.0 200 OK\r\n");
		send(sock,line,strlen(line),0);
		sprintf(line,"Content-Type:text/html\r\n");
		send(sock,line,strlen(line),0);
		sprintf(line,"\r\n");
		send(sock,line,strlen(line),0);

		while(read(output[0],&c,1)>0){
			send(sock,&c,1,0);
		}
		waitpid(id,NULL,0);
		//等待子进程 防止僵尸进程
		close(input[1]);
		close(output[0]);
	}
	return 200;
}
void* handlerRequest(void *arg){
	int sock=(int)arg;
	char line[MAX];
	char method[MAX/16];
	char url[MAX];
	char path[MAX];

	char *query_string=NULL;

	int i,j;
	int status_code=200;
	int cgi=0;
	getLine(sock,line,MAX);
	printf("line:%s\n",line);
	i=0;
	j=0;
	while(i<sizeof(method)-1&&j<sizeof(line)&&!isspace(line[j])){
		method[i]=line[j];
		i++,j++;
	}
	method[i]=0;
	//Get,Post,gEt,pOst
	printf("method:%s\n", method);
	if(strcasecmp(method,"GET")==0){

	}else if(strcasecmp(method,"POST")==0){
		cgi=1;
	}
	else{
		printf("method error...\n");
		status_code=400;
		clearHeader(sock);
		goto end;
	}
	while(j<sizeof(line)&&isspace(line[j])){
		j++;
	}
	i=0;
	while(i<sizeof(url)-1&&j<sizeof(line)&&!isspace(line[j])){
		url[i]=line[j];
		i++,j++;
	}
	url[i]=0;
	printf("url:%s\n", url);
	//method,url.cgi=?
	//判断是否带参   带参 ？
	if(strcasecmp(method,"GET")==0){
		query_string=url;
		while(*query_string!='\0'){
			if(*query_string=='?'){
				cgi=1;
				*query_string='\0';
				query_string++;
				break;
			}
			query_string++;
			//没带参 不是cgi
		}
	}
	//method url get（cgi=1？querystring0
	sprintf(path,"wwwroot%s",url);
	if(path[strlen(path)-1]=='/'){
		strcat(path,"index.html");
	}
	//判断www//index.html
	//method,url,GET(cgi=1?query_string)
	printf("method:%s\n",method);
	printf("path:%s\n",path);
	printf("cgi:%d\n",cgi);
	printf("query_string:%s\n",query_string);

	struct stat st;
	//判断文件是否存在 成功返回0
	if(stat(path,&st)<0){
		status_code=404;
		clearHeader(sock);
		goto end;
	}else{
		if(S_ISDIR(st.st_mode))
			//权限 第一列十个第一个类型
		{
			strcat(path,"/index.html");
		}
		else if((st.st_mode&S_IXUSR)||(st.st_mode&S_IXGRP)||(st.st_mode&S_IXOTH))
			//二进制文件跑起来返回浏览器  执行权限IXUSR 00100   IXGRP  00010   IXOTH 00001
		{
			cgi=1;
		}else{
			//do nothing
		}
		if(cgi){
			printf("exe_cgi\n");
			//post get 带参
			status_code=exe_cgi(sock,method,path,query_string);
		}else{
			printf("echo_www\n");
			//非cgi 绝对get
			status_code=echo_www(sock,path,st.st_size);
		}
		//是否cgi
	}
end:
	if(status_code!=200){
		echoError(sock,status_code);
	}
	close(sock);
}

int startup(int port){
	int sock=socket(AF_INET,SOCK_STREAM,0);
	if(sock<0){
		perror("socket");
		exit(2);
	}
	int opt=1;
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	struct sockaddr_in local;
	local.sin_family=AF_INET;
	local.sin_port=htons(port);
	local.sin_addr.s_addr=htonl(INADDR_ANY);

	if(bind(sock,(struct sockaddr*)&local,sizeof(local))<0){
		perror("bind");
		exit(3);
	}
	if(listen(sock,10)<0){
		perror("listen");
		exit(4);
	}
	return sock;
}
//./http 9999
int main(int argc,char *argv[])
{
	if(argc !=2){
		usage(argv[0]);
		return 1;
	}
	signal(SIGPIPE,SIG_IGN);
	int listen_sock=startup(atoi(argv[1]));

	for(;;){
		struct sockaddr_in client;
		socklen_t len=sizeof(client);

		int sock=accept(listen_sock,(struct sockaddr*)&client,&len);
		if(sock<0){
			perror("accept");
			continue;
		}
		printf("get a new connect...!\n");
		pthread_t tid;
		pthread_create(&tid,NULL,handlerRequest,(void*)sock);
		pthread_detach(tid);
	}
	return 0;
}

