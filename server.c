#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<pthread.h>
#include<errno.h>
#include<signal.h>
#define MAX 20
#define MAXLINE 20          //listen最大等待队列
#define PORT 7230           //端口号
typedef struct group{//所有群信息

	char builder[100] ;
	char g_name[100] ;
}group_t; 
char *a_g_f ="all_group";
pthread_mutex_t mutex ;//设置锁
typedef struct b{           //用户各种信息
    int fd;
	int size ;
    int flag;
    int login;
    int power;
    char gr_name[20];//群名称
    char txt[100];
    char number[10];
    char passwd[20];
    char object[10];
    char pathname[100];
    char buf[10000];
}user;

typedef struct a{       //每个人的好友
    char number[10];
    struct a *next;
}fri;

typedef struct e{           //群内成员
    char number[10];
    struct e *next;
}gro;

typedef struct c{        //所有在线用户的信息
    int fd;
    int flag;           //标记是否在线
    char number[10];
    char passwd[20];
    struct c *next;
}peo;

typedef struct d{         //用户的离线消息
    char buf[4096];
    struct d *next;
}off;

int i = 1;
time_t *timep;
user people;
peo *head;
fri *phead;
off *ohead;
gro *ghead;
char number[50];  //备份自己账号
void epoll_sock(int sock_fd);
int u_exist(user * people);//判断好友存在与否
int g_exist(user* people);//判断群是否存在
void show_group(int conn_fd);//群列表
void get_alljgroup();//获取用户已加入的群信息
void member_group( int conn_fd );
int is_member(char* g_name);
void online_remind( char *number );
void take_f_info();//将群内所有用户倒入链表
void offline_remind( char *number );//用户下线提醒
void send_file();//发送文件
void ask( int conn_fd );
void take_chatlog( int conn_fd );
void save_talklog( user *people );
void invite( user *people );
void take_group();
void group_chat();
void save_log( char *number );
void del_friend( char *number,char *nofri );
void private_chat();
void wenjian4( user *people );
void wenjian3( user *people );
void wenjian2( char *number );
void wenjian1( char *number );
void look_fri();
void send_offline( int conn_fd );
void take_offline( char *number );
void off_line( user *people,char *number );
int check_friend( char *number );
int check_line( char *number );
void take_friend( char *p );
void save_friend( char *number,char *friend );
void off_lines( int conn_fd );
void reply( user *people );
void take_out();
int check_login( user *people,int conn_fd );
int check_setin( user *people);
void save();
void add_friend( user *people,int conn_fd );
void *user_process();
void  save_group(user* people);
int epoll_fd ;

void epoll_sock(int sock_fd)//注册epoll可读事件
{
    int ret , i;      //同样用来处理返回值,epoll_fd为全局变量
    epoll_fd = epoll_create(256);                
    if(epoll_fd < 0)
        printf("\n\t\tepoll_create  %d",__LINE__);

    struct epoll_event ep_ev;                    
    ep_ev.events = EPOLLIN;    //设置事件为可读事件                  
    ep_ev.data.fd = sock_fd;                   
    ret = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock_fd,&ep_ev);
    if(ret < 0)
        printf("\n\t\tepoll_ctl  %d",__LINE__);

    struct epoll_event evs[MAX];              //设置epoll可以监听的描述符个数
	int max = MAX ;
    int epoll_ret = 0;                                //epoll专用返回的事件发生数
    while(1)                                      //在永真循环中,一直用epoll管理
    {
        epoll_ret = epoll_wait(epoll_fd,evs,max,-1);   //epoll_wait获取返回值

        switch(epoll_ret)
        {
            case -1:
                printf("出错\n");
                break;
            case 0:
                printf("\n\t\t没有监听事件!\n");
                break;
            default:
                for(i = 0 ; i < epoll_ret ; i++)
                {
                    if((evs[i].data.fd == sock_fd) && (evs[i].events & EPOLLIN))   //判断是否为监听套接字且为读事件
                    {
                        struct sockaddr_in cli_addr;
                        socklen_t          cil_len;
                        cil_len = sizeof(struct sockaddr_in); 
                        /*进行套接字的连接*/
                        int conn_fd = accept(sock_fd,(struct sockaddr *)&cli_addr,&cil_len);
                        if(conn_fd < 0)
                            printf("\n\t\t连接出错  %d",__LINE__);

                        ep_ev.events = EPOLLIN | EPOLLONESHOT; //epoll读事件
                        ep_ev.data.fd = conn_fd;               //epoll处理的是连接套接字 
                        ret = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,conn_fd,&ep_ev);
                        if(ret < 0)
                        {
                            printf("\n\t\t注册出错 %d" ,__LINE__);
                            exit(0);
                        } 
                    }
                    else                      //如果并非是监听套接字,而是其他的连接套接字
                    {
                        if(evs[i].events & EPOLLIN)                // &运算判断其是否为可读事件
                        {
                            pthread_t thid;                        //线程ID

                            /*创建线程,并且传入连接套接字,进行处理*/
                            pthread_create(&thid,NULL,(void *)user_process,(void *)&evs[i].data.fd);

                            pthread_detach(thid);                 //在线程外部调用,回收资源
                        }
                    }   
                }
                break;
        }
    }
	close(sock_fd);
}   

int main()
{

	pthread_mutex_init(&mutex , NULL) ;
    signal( SIGPIPE,SIG_IGN );
    pthread_t tid;
    struct sockaddr_in cin,sin;
    socklen_t sin_len;
    int conn_fd,sock_fd;
    int optval;
    
    memset( &cin,0,sizeof(cin) );       //置0

    cin.sin_family = AF_INET;
    cin.sin_port = htons(PORT);
    cin.sin_addr.s_addr = htonl(INADDR_ANY);

    //创建套接字
    if( (sock_fd = socket(AF_INET,SOCK_STREAM,0)) < 0 )
    {
        printf( "socket error\n" );
        exit(0);
    }

    //设置该套接字市值可以重新绑定端口
    optval = 1;
    if( setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,(void *)&optval,sizeof(int)) < 0 )
    {
        printf( "setsockopt error\n" );
    }

    //绑定套接字到本地端口
    if( bind(sock_fd,(struct sockaddr *)&cin,sizeof(struct sockaddr)) < 0 )
    {
        printf( "bind error\n" );
        exit(0);
    }
	printf("\n\t\t开始服务......\n");
    //化为监听套接字
    if( listen(sock_fd,MAXLINE) < 0 )
    {
        printf( "listen error\n" );
        exit(0);
    }
	printf( "\n\t\tListening....\n" );
    sin_len = sizeof( struct sockaddr_in );
	head = (peo *)malloc( sizeof(peo) ); //初始化链表头指针
    head->next = NULL;
    phead = (fri *)malloc( sizeof(fri) );//好友链表
    phead->next = NULL;
    ohead = (off *)malloc( sizeof(off) );//下线用户链表
    ohead->next = NULL;
    ghead = (gro *)malloc( sizeof(gro) );//群成员链表
    ghead->next = NULL;
	take_out();  //创建链表取出用户信息存入链表
    int count = 0; 	
	epoll_sock(sock_fd);	
}

void *user_process( void *arg )        //主要函数，调用子函数，进行各种功能都在这里调用子函数完成
{
    printf( "\n" );
    int ret,flag;
    memset( &people,0,sizeof(people) );
    int conn_fd = *(int *)arg;
    strcpy( number,people.number );

    //接收数据
    while(1)
    {
        ret = recv( conn_fd,(void *)&people,sizeof(user),0);    //接受信息
    	
            if(ret == 0)
            {
                off_lines( conn_fd );//检测用户是否下线
				close(conn_fd);
                pthread_exit( NULL );

            }
        
        if( people.login == 2 )   //请求登录注册
        {
            if( people.flag == 1 ) //登录
            {
                flag = check_login( &people,conn_fd );
                if( flag == 0 )
                {
                    send( conn_fd,(void *)&flag,sizeof(flag),0 );
                }
                else
                {
					flag = 1 ;
                    send( conn_fd,(void *)&flag,sizeof(flag),0 );

                    take_offline( people.number );   //检查离线消息
                    if( ohead->next != NULL )
                    {
                        memset( people.buf,0,sizeof(people.buf) );
                        off *p = ohead->next;
                        people.login = 0;
                        while(p) 
                        {
                            memset( people.buf,0,sizeof(people.buf) );
                            strcpy( people.buf,p->buf );
                            p = p->next;
                            send( conn_fd,(void *)&people,sizeof(people),0 );
                        }
                    }
                }
            }

            else if( people.flag == 2 ) //注册
            {
                flag = check_setin( &people );
                if( flag == 1 )     //注册成功了，保存
                {
                    save(head);
                }
                else
                {
                    memset( &people,0,sizeof(people) );
                }
                send( conn_fd,(void *)&flag,sizeof(flag),0 );
            }
        }

        if( people.login == 1 || people.login == 11 )         //请求添加好友
        {

            add_friend(&people,conn_fd);
        }
        
        if( people.login == 22 )    //请求展示好友列表
        {
            take_friend( people.number );        //从文件读取该用户的好友
            look_fri();
            send( conn_fd,(void *)&people,sizeof(people),0 );
        }

        if( people.login == 3 )     //请求私聊
        {
            private_chat();
        }
        
        if( people.login == 42 )        //请求建群
        {
            wenjian3( &people );
			
			send(conn_fd , (void *)&people , sizeof(people) ,0);
        }
        if( people.login == 43)         //请求群聊
        {
            take_group();
            group_chat();
        }
        if(people.login == 80){ //展示群列表
		
			show_group(conn_fd);
		}
			
			
        if( people.login == 45 )        //请求查看群成员
        {
            take_group();
            member_group( conn_fd );
        }

        if( people.login == 46 )        //请求解散群
        {
            char tmp[50] = {0};
            strcpy( tmp,people.object );
            strcat( tmp,"group" );
           remove( tmp );
        }
		if(people.login == 101){//用户加入的所有群信息
		
			get_alljgroup();
			send(conn_fd ,(void*)&people , sizeof(people) , 0);

		}
        if( people.login == 44 )    //邀人进群
        {
			take_friend(people.number);
           invite( &people ); 
		   send(conn_fd ,(void*)&people ,sizeof(people),0);
        }

        if( people.login == 7 ) //请求删除好友
        {
            take_friend( people.number );
            del_friend( people.number,people.buf );
            take_friend( people.number );
            del_friend( people.buf,people.number );
        }

        if( people.login == 8 )     //请求查看聊天记录
        {
            take_chatlog( conn_fd );
        }
        
        if( people.login == 9 )     //请求文件传输
        {
            ask(conn_fd);
        }
        if( people.login == 99 )    //对于文件传输的回应
        {
            ask(conn_fd);
        }
        if( people.login == 999 )   //传输文件
        {
            send_file();
        }
    }
    pthread_exit(0);
}

void get_alljgroup(){//找用户已加入的群组信息

	FILE* fp ;
	fp = fopen(a_g_f ,"r");
	if(fp == NULL){
		strcpy(people.buf , "\n\t\t你还没有加入群组!\n");
		return ;
	}
	group_t p ;
	bzero(&(people.buf),sizeof(people.buf));
	strcat(people.buf ,"\n\t\t");
	int flag = 0 ;
	while(1){
	
		if(fread(&p , sizeof(p) , 1 , fp) < 1){
			break ;
		}
		if(is_member(p.g_name)){
			flag = 1;
			strcat(people.buf ,p.g_name);
			strcat(people.buf ,"\n");
		}
	}
	fclose(fp) ;
	if(flag == 0){

		strcpy(people.buf , "\n\t\t你还没有加入群组!\n");

	}
}

int is_member(char *g_name){//根据群名称打开群文件查看群成员

	char member[100] ;
	strcat(g_name ,"group") ;
	FILE *fp ;
	fp =fopen(g_name ,"r");
	if(fp == NULL){
		printf("\n\t\t该群可能不存在\n");
		return 0 ;
	}
	while(fscanf(fp , "%s" ,member) !=EOF){
		
		if(strcmp(member ,people.number) == 0){//如果群内成员和用户名一至
			fclose(fp) ;
			return 1 ;
		}
	}
	fclose(fp);
	return 0 ;

}
void take_out()				//从文件读取用户信息到链表 
{
    int t = 0,count = 0;
	FILE *fp;
	fp=fopen("all_user","r");
	if(fp == NULL)
	{
		printf("open file error!");
		exit(0);
	}
	peo *p1,*p2,*p3;
	p2=p1=(peo *)malloc( sizeof(peo) );
	head->next = p1;
    p3 = head;
    rewind( fp );   //确保文件指针在开头
	while(fscanf(fp,"%d %d %s %s",&p1->fd,&p1->flag,p1->number,p1->passwd)!=EOF)
	{
        t = 1;
		p1=(peo *)malloc( sizeof(peo) );
		p2->next = p1;
		p2=p1;
        p3 = p3->next;
	}
    if( t == 0 )
    {
        head->next = NULL;
    }
    p3->next = NULL;
    p1 = NULL;
    p2 = NULL;
    fclose(fp);
}

void save()           //保存用户信息到文件
{
    FILE *fp;
	fp=fopen("all_user","w");
    if(fp == NULL)
    {
        printf("save error");
        exit(0);
    }
    peo *p=head->next;
	while(p)
    {
        fprintf(fp,"%d %d %s %s\n",p->fd,p->flag,p->number,p->passwd);
        p=p->next;
    }
    fclose(fp);
}




int check_setin( user *people)   //注册账号密码写入链表
{

    peo *p = head->next;
    peo *p2 = head;
    peo *p1=(peo *)malloc( sizeof(peo) );
    
    while( p )
    {
        if( strcmp(p->number,people->number) == 0 )
        {
            printf( "账号已被使用\n" );
            return 0; 
        }
        p2 = p2->next;
        p = p->next;
    }

    strcpy( p1->number,people->number );
    strcpy( p1->passwd,people->passwd );
    p1->fd = 0;
    p1->flag = 0;
    p2-> next = p1;
    p1->next =NULL;

    return 1;
 }

int check_login( user *people,int conn_fd )       //登录
{
    int flag = 0;
    peo *p = head->next;
    while( p)
    {
        if( (strcmp(p->number,people->number)) == 0 )       //账号存在
        {
            flag = 1;
            break;
        }
        p = p->next;
    }
    if( flag == 0 )
    {
        printf ("账号不存在\n");
        return 0;
    }
    if( (strcmp(p->passwd,people->passwd)) != 0 )
    {
        printf( "密码不正确\n" );
        return 0;
    }
    
    if( p->flag == 1 )
    {
        return 0;
    }
	printf("密码：%s\n",people->passwd);
    printf( "成功登录\n" );
    char *tmp = "成功登录";
    save_log(tmp);
    
    wenjian1( p->number );          //各种需要的文件
    wenjian2( p->number );
    online_remind( p->number );     //上线提醒

    p->flag = 1;   //登陆成功即在线
    p->fd = conn_fd;  //保存套接字

    return 1;
}

void  off_lines( int conn_fd )         //若有用户下线　链表里的fd置为-1
{
    peo *p = head->next;
    int t = 0;
    while( p )
    {
        if( p->fd == conn_fd )
        {
            t = 1;
            break;
        }
        p = p->next;
    }
    if( t == 0 )
    {
        return ;
    }  
    p->fd = 0;
    p->flag = 0;

    offline_remind( p->number );        //下线提醒

    return ;
}
int check_line( char *number )     //检查对方是否离线
{
    peo *p = head->next;
    while( p )
    {
        if( strcmp( p->number,number ) == 0 && p->flag == 0)     //离线
        {
            return 0;
        }
        else if( strcmp( p->number,number ) == 0 && p->flag == 1 )   //在线
        {
            return 1;
        }
        p = p->next;
    }
    return 0;    //没有这个账号
}


void  reply( user *people )     //对于添加好友的回复
{
    peo *p = head->next;
    
    while( p )
    {
        if( p->fd == people->fd )
        {
            break;
        }
        p = p->next;
    }

    if( strcmp(people->buf,"y") == 0 )
    {
        save_friend( people->number,p->number );    //保存他到你的好友
        save_friend( p->number,people->number );

        memset( people->buf,0,sizeof(people->buf) );
        strcpy(people->buf,"\n\t\t你成功添加");
        strcat(people->buf,people->number);
        strcat(people->buf,"为好友,现在可以聊天了\n");
        send(people->fd,people,sizeof(user),0);
    }
    else if( strcmp(people->buf,"n") == 0 )
    {
        memset( people->buf,0,sizeof(people->buf) );
        strcpy(people->buf,"\n\t\t你的请求被");
        strcat(people->buf,people->number);
        strcat(people->buf,"拒绝了\n");
        send( people->fd,people,sizeof(user),0 );
    }
        
}

void add_friend( user *people,int conn_fd )      //添加好友
{
    
    take_friend( people->number );
    
    fri *pp = phead->next;
    while( pp )
    {
        pp = pp->next;
    }

    if( people->login == 11 )
    {
        reply( people );
        return ;
    }
    int t = 0;
    peo *p = head->next;
    while( p )        //找该账号
    {
        if( strcmp( p->number,people->object ) == 0 )          
        {
            t = 1;
            break;
        }
        p = p->next;
    }
    if( t == 0 )
    {
        memset( people->buf,0,sizeof(people->buf) );   
        strcpy( people->buf,"\n\t\t该用户不存在！\n" );
        people->login = 111;

        send( conn_fd,(void *)people,sizeof(user),0 );
        return ;
    }
    else 
    {
        int ret;
        ret = check_friend( people->object );  //检查是否已经添加对方为好友
        if( ret == 1 )
        {
            memset( people->buf,0,sizeof(people->buf) );
            strcpy( people->buf,"对方已经是你的好友" );
            people->login = 111;

            send( conn_fd,(void *)people,sizeof(user),0 );
            return ;
        }

        memset( people->buf,0,sizeof(people->buf) );
        strcat( people->buf,people->number );
        strcat( people->buf," 想要添加你为好友" );
        people->fd = conn_fd;

        ret = check_line( p->number );  //检查对方是否在线
        if( ret == -1 )      //无该账号
        {
            return ;
        }
        if( ret == 0 )    //离线，把消息存起来
        {
            off_line( people,p->number );
            return ;
        }
        
        if( send( p->fd,people,sizeof(user),0 ) < 0) //给想添加的账号发送请求
        {
            printf( " 原来是这里错了\n" );
        }
    }
}

void save_friend( char *number,char *friend )      //保存每个账号的好友到文件
{
    FILE *fp;
    fp = fopen( number,"a+" );
    if( fp == NULL )
    {
        printf( "save_friend fopen error\n" );
        return ;
    }
    fprintf( fp,"%s\n",friend );
    fclose( fp );
}

void  take_friend( char *p )         //从文件读取每个人的好友
{
    int t = 0;
    FILE *fp;
	fp=fopen( p,"r" );
	if(fp == NULL)
	{
		printf("take_friend error\n");
		return ;
	}
	fri *p1,*p2,*p3;
	p2=p1=(fri *)malloc( sizeof(fri) );
	phead->next = p1;
    p3 = phead;
    rewind( fp );    //确保文件指针在开头
	while( fscanf(fp,"%s",p1->number) != EOF )
	{
        t = 1;
		p1=(fri *)malloc( sizeof(fri) );
		p2->next = p1;
		p2=p1;
        p3 = p3->next;
	}
    if( t == 0 )
    {
        phead->next = NULL;
    }
    p3->next = NULL;
    p1 = NULL;
    p2 = NULL;
    fclose(fp);
    
}

int check_friend( char *number )          //检查是否已经添加对方为好友
{
    fri *p = phead->next;
    while( p )
    {
        if( strcmp( p->number,number ) == 0 )
        {
            return 1;
        }
        p = p->next;
    }
    return 0;
}

void off_line( user *people,char *number )     //保存该用户离线消息到文件
{
    char p[50]={0};
    strcpy( p,number );
    strcat( p,"off-line" );
    FILE *fp;
    fp = fopen( p,"a" );
    if( fp == NULL )
    {
        printf( "off_line fopen error\n" );
    }
    fprintf( fp,"%s\n",people->buf );
    fclose(fp);
}

void take_offline( char *number )           //从文件读取离线信息到链表
{
    char ch;
    int i = 0;
    char p[50]={0};
    strcpy( p,number );
    strcat( p,"off-line" );

    off *p1,*p2,*p3;
    p1 = (off *)malloc( sizeof(off) );
    p2 = ohead;
    p3 = ohead;
    
    int t = 0;
    FILE *fp;
    fp = fopen( p,"r" );
    if( fp == NULL )
    {
        printf( "take_offline fopen error\n" );
    }

    rewind(fp);
    while( fscanf( fp,"\n%[^\n]",p1->buf ) != EOF )
    {
        t = 1;
        p2->next = p1;
        p2 = p1;
        p1 = (off *)malloc( sizeof(off) );
        p3 = p3->next;
    }
    if( t == 0 )
    {
        ohead->next = NULL;
    }
    p3->next = NULL;
    p1 = NULL;
    p2->next = NULL;
    
    fp = fopen( p,"w+" );
    fclose( fp );
}


void look_fri()         //把好友信息放到buf里
{
    fri *p = phead->next;
    printf( "\n" );
    memset( people.buf,0,sizeof(people.buf) );
    while( p )
    {
        strcat( people.buf,p->number );
        strcat( people.buf,"\n\t\t" );
        p = p->next;
    }
}

void wenjian1( char *number )  //该文件存放好友账号
{
    char p[50]={0};
    strcpy( p,number );
    FILE *fp;
    if( (fp = fopen(p,"r")) == NULL )
    {
        fp = fopen( p,"wa+" );
    }
    fclose(fp);
}
void wenjian2( char *number )       //该文件存放离线消息
{
    char p[50]={0};
    strcpy( p,number );
    strcat( p,"off-line" );
    FILE *fp;
    if( (fp = fopen( p,"r" )) == NULL )
    {
        fp = fopen( p,"w+" );
    }
    fclose(fp);
}
//save_groupinfo
void save_group(user* people){//将创建的群组和群主保存

	FILE * fp ;
	pthread_mutex_lock(&mutex);
	group_t p ;
	strcpy(p.builder ,people->number);
	strcpy(p.g_name , people->buf);
	if(access(a_g_f , 0)){//如果文件不存在，以写形式是创建文件
		fp = fopen(a_g_f,"wb+");
		if(fp == NULL){
			printf("\n\t\tsave_group error!\n");
			return  ;
		}
	}//否则的话就追加的方式添加信息
	else{
		fp = fopen(a_g_f , "ab+");
		if(fp == NULL){
			printf("\n\t\tsanve_a_g_f error!\n");
			return ;
		}
	}
	fwrite(&p , sizeof(p) , 1,fp);
	fclose(fp);
	pthread_mutex_unlock(&mutex);
}
void show_group(int conn_fd){//显示群列表

	FILE * fp ;
	if(access(a_g_f ,0)){
		
		strcpy(people.buf,"\n\t\t你还没有创建群组！\n");
		send(conn_fd ,(void *)&people , sizeof(people) , 0 );
		return ;
	}
	fp =fopen(a_g_f ,"r");
	group_t q ;
	int flag = 0;
	bzero(&(people.buf),sizeof(people.buf));
	strcat(people.buf ,"\n\t\t");
	while(1){
	
		if(fread(&q ,sizeof(q) , 1, fp ) < 1){
			break ;
		}
		if(strcmp(q.builder , people.number) == 0){
		
			flag = 1 ;
			strcat(people.buf , q.g_name);
			strcat(people.buf , "\n\t\t");
		}
	}
	if(flag == 0){
	
		strcpy(people.buf , "\n\t\t你还没有创建群!\n");
		printf("将发送的命令是:%d",people.login);
		send(conn_fd ,(void *)&people , sizeof(people),0);
	}
	else{
		send(conn_fd , (void *)&people , sizeof(people), 0);
	}
	fclose(fp);
}
int g_exist(user* people){//检查群是否已创建
	
	FILE *fp ;
	if(access(a_g_f , 0)){
		return 0 ;
	}
	fp =fopen(a_g_f , "r");
	group_t p ;
	while(1){
		
		if(fread(&p , sizeof(p) , 1 , fp) < 1){
			break ;
		}
		if((strcmp(people->buf ,p.g_name) == 0)&& (strcmp(people->number , p.builder) == 0))
		{
			fclose(fp);
			return 1 ;
		}
	}
	fclose(fp);
	return 0 ;
}

void wenjian3( user *people )    //建文件存放群成员
{
	

	if(g_exist(people)== 1){//如果群存在，提醒用户重新设置
		
		strcpy(people->buf , "\n\t\t这个群已经存在!\n");
		return ;
	}

    char p[50] = {0};
	save_group(people);
    strcpy( p,people->buf );

    strcat( p,"group" );
    FILE *fp;
    if( (fp = fopen(p,"r+")) == NULL )
    {
        fp = fopen( p,"w+");
    }
    
    fprintf( fp,"%s\n",people->number );

	strcpy(people->buf , "\n\t\t创建群聊成功!你可以邀请好友了!\n");
    fclose(fp);
}

void wenjian4( user *people )     //创建文件保存聊天记录
{
    FILE *fp;
    char tmp[50] = {0};
    strcmp( tmp,people->number );
    strcat( tmp,"chat-log" );

    if( (fp = fopen(tmp,"r+")) == NULL )
    {
        fp = fopen( tmp,"w+" );
    }
    fclose(fp);
}

void private_chat()     //私聊
{
    int ret;
    peo *p = head->next;
    char tmp[4096],*s;
    timep = malloc( sizeof(*timep) );  //获取当前时间 
    time( timep );
    s = ctime( timep ); 
    while( p )
    {
        if( strcmp( p->number,people.object ) == 0 )
        {
            break;
        }
        p = p->next;
    }
    memset( tmp,0,sizeof(tmp));
    strcpy( tmp,s );
    strcat( tmp,"  " );
    strcat( tmp,people.number );
    strcat( tmp,"  says:   " );
    strcat( tmp,people.buf );
    strcpy( people.buf,tmp );

    ret = check_line( p->number );   //检查是否在线
    if( ret == 0 )
    {
        off_line( &people,p->number );
        return ;
    }

    wenjian4( &people );
    save_talklog( &people );

    send( p->fd,(void *)&people,sizeof(people),0 );  //在线就发送
    
}

void del_friend( char *number,char *nofri )     //删除好友
{

    FILE *fp;
    fp = fopen( number,"w" );
    int t = 0;
    fri *p1 = phead->next;
    fri *p2 = phead;
    fri *p3 = phead->next;
    while( p1 )
    {
        if( strcmp( p1->number,number ) == 0 )
        {
            t = 1;
            break;
        }
        p1 = p1->next;
        p2 = p2->next;
    }
    if( t == 0 )
    {
        return ;
    }
    else
    {
        p2->next = p1->next;
        free( p1 );
    }
    
    while( p3 )
    {
        fprintf( fp,"%s\n",p3->number );
        p3 = p3->next;
    }

}

void save_log( char *number )    //日志文件
{
    FILE *fp;
    fp = fopen( "log","a" );
    if( fp == NULL )
    {
        printf( "log fopen error \n" );
    }
    char *s;
    char tmp[500];
    timep = malloc( sizeof(timep) );
    time( timep );
    s = ctime(timep);
    memset(tmp,0,sizeof(tmp));

    strcat( tmp,s );  
    strcat( tmp,"=====>" );
    strcat( tmp,number );
    
    fprintf( fp,"%s\n",tmp );
    fclose(fp);
    return ;
}

void take_group()   //取出群成员存到链表
{

    FILE *fp;
    char p[50] = {0};
    int t = 0;

    gro *p1 = (gro *)malloc( sizeof(gro) );
    p1->next = NULL;
    gro *p2 = ghead;
    gro *p3 = ghead;

    strcpy( p,people.object );

    strcat( p,"group" );

    fp = fopen( p,"r" );
    if( fp == NULL )
    {
        printf ("group_chat fopen error \n");
        return ;
    }
    while( fscanf( fp,"%s",p1->number ) != EOF )
    {
        t = 1;
        p2->next = p1;
        p2 = p1;
        p1 = (gro *)malloc( sizeof(gro) );
        p3 = p3->next;
    }
    if( t == 0 )
    {
        ghead->next = NULL;
    }
    p1 = NULL;
    p2->next = NULL;
    p3->next = NULL;

    fclose( fp );
}

void group_chat()           //群聊
{
	
    gro *p = ghead->next;
    char buf[4096];
    strcpy( buf,people.buf );
    while( p )
    {
        strcpy( people.buf,buf );
        strcpy( people.object,p->number );
        private_chat();
        p = p->next;
    }
}

int u_exist(user* people){//判断邀请的好友是否存在
	
	
	fri* p ;
	p = phead->next ;
	if(p == NULL)return 0 ;
	while(p){
		
		if(strcmp(p->number ,people->object) == 0){
				return 1 ;
		}
		p =p->next ;
	}
	return 0 ;  
}//备注：还需判断进的群是否存在
//判断好友在不在这个群里
void take_f_info(){//将群里的成员倒入链表
	FILE * fp ;
	char p[30] = {0};
	int t = 0 ;
	gro* p1 = (gro*)malloc(sizeof(gro));
	p1->next = NULL ;
	gro *p2 = ghead ;
	gro* p3 = ghead ;
	strcpy(p , people.buf);
	strcat(p,"group");
	fp =fopen(p, "r");

	if(fp == NULL){
	
		printf("group file open error!\n");
		
	}
	while(fscanf(fp , "%s" ,p1->number) !=EOF){
	
		t = 1 ;
		p2->next = p1 ;
		p2 = p1 ;
		p1 = (gro*)malloc(sizeof(gro));
		p3 = p3->next ;
	}
	if(t == 0){
	
		ghead->next = NULL ;
	
	}


	p1 = NULL ;
	p2->next = NULL ;
	p3->next = NULL ;
	fclose(fp);
}
//take_friend
int is_ingroup(){
	take_f_info();

	gro * p ;
	p =ghead->next ;
	if(p == NULL){
		return 0 ;
	}
	while(p){

		if(strcmp(p->number ,people.object) == 0)return 1 ;
	
		p = p->next ;
	
	}
	return 0 ;
	
}
void invite( user *people )     //邀人进群
{
	if(u_exist(people) == 0){	//判断有无这一好友
		strcpy(people->buf , "\n\t\t查无此好友!\n");
		return ;
	}
	if(g_exist(people)== 0){//判断群是否存在
		strcpy(people->buf,"\n\t\t你没有创建这个群！\n");
		return ;
	}
	if(is_ingroup() == 1){//判断该用户是否在群中
		strcpy(people->buf,"\n\t\t该好友已是群成员\n");
		return ;
	}
    FILE *fp;
    char p[50] = {0};
    strcpy( p,people->buf );
    strcat( p,"group" );

    fp = fopen( p,"a" );
    if( fp == NULL )
    {
        printf( "无这个群\n" );
        return ;
    }
    fprintf( fp,"%s\n",people->object );
	strcpy(people->buf , "\n\t\t你邀请了");
	strcat(people->buf , people->object);
	strcat(people->buf , "进入该群聊!\n");
    fclose(fp);
}

void save_talklog( user *people )     //保存聊天记录
{
    char tmp[50] = {0};
    char buf[4096] = {0};
    FILE *fp;
    strcpy( tmp,people->number );
    strcat( tmp,"chat-log" );
    strcpy( buf,people->buf );
    fp = fopen( tmp,"a" );
    if( fp == NULL )
    {
        printf( "save_talklog fopen error\n" );
        return ;
    }
    
    strcat( buf,"   " );
    strcat( buf,"      to  " );
    strcat( buf,people->object );
    strcat( buf,"\n" );
    fprintf( fp,"%s",buf );

    fclose(fp);

}

void take_chatlog( int conn_fd )     //查看聊天记录
{
    FILE *fp;
    char tmp[50] = {0};
    char buf[4096] = {0};
    strcpy( tmp,people.number );
    strcat( tmp,"chat-log" );

    fp = fopen( tmp,"r" );
    if( fp == NULL )
    {
        printf( "take_chatlog fopen error\n" );
        return ;
    }
    memset( people.buf,0,sizeof(people.buf) );
    while( fscanf( fp,"\n%[^\n]",buf ) != EOF)
    {
		strcat(people.buf ,"\n\t\t");

        strcat( people.buf,buf );
    }
    send( conn_fd,(void *)&people,sizeof(people),0 );
    fclose(fp);
}

void ask( int conn_fd )      //请求传输文件
{
    int ret;
    peo *p = head->next;


    if( people.login == 9 )
    {
        while( p )
        {
            if( strcmp( p->number,people.object ) == 0 )
            {
                break;
            }
            p = p->next;
        }
        people.fd = conn_fd; 
        send( p->fd,(void *)&people,sizeof(people),0 );
    }
    if( people.login == 99 )
    {
        send( people.fd,(void *)&people,sizeof(people),0 );
    }

}

void send_file()       //文件传输
{
    peo *p =head->next;
    while( p )
    {
        if( strcmp( p->number,people.object ) == 0 )
        {
            break;
        }
        p = p->next;
    }
    int ret;
    ret = check_line( p->number );
    if( ret == 0 )
    {
        return ;
    }
    send( p->fd,(void *)&people,sizeof(people),0 );
}

void online_remind( char *number )      //发送好友上线提醒
{
    int ret;
    char tmp[100] = {0};
    people.login = 123;
    take_friend( number );
    fri *p = phead->next;
    peo *p1;

    strcpy( tmp,"你的好友" );
    strcat( tmp,number );
    strcat( tmp,"已经上线" );
    strcpy( people.buf,tmp );
    while( p )
    {
        p1 = head->next;

        ret = check_line( p->number );      //如果对方不在线不发送
        if( ret == 0 )
        {
            p = p->next;
            continue;
        }
       while( p1 )
        {
            if( strcmp(p->number,p1->number) == 0 )
            {
                break;
            }
            p1 = p1->next;
        }
        send( p1->fd,(void *)&people,sizeof(people),0 );
        
        p = p->next;
    }
}
//open
void offline_remind( char *number )     //发送好友下线提
{
    int ret;
    peo *p1;
    char tmp[100] = {0};
    people.login = 456;
    take_friend( number );
    fri *p = phead->next;

    strcpy( tmp,"你的好友" );
    strcat( tmp,number );
    strcat( tmp,"已经下线" );
    strcpy( people.buf,tmp );

    while( p )
    {
        p1 = head->next;
        ret = check_line( p->number );
        if( ret == 0 )
        {
            p = p->next;
            continue;
        }
        while( p1 )
        {
            if( strcmp(p1->number,p->number) == 0 )
            {
                break;
            }
            p1 = p1->next;
        }
        send( p1->fd,(void *)&people,sizeof(people),0 );
        
        p = p->next;
    }
}

void member_group( int conn_fd )         //展示群成员
{
    gro *p = ghead->next;
    while( p )
    {
        memset(people.buf,0,sizeof(people.buf));
        strcpy( people.buf,p->number );
        send( conn_fd,(void *)&people,sizeof(people),0 );
        p = p->next;
    }
}

