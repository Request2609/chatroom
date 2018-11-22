#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/stat.h>
#include<netinet/in.h>
#include<pthread.h>
#include<arpa/inet.h>
#include<signal.h>
#define MAXLEN 1000 
#define PORT 7230
#define IP "127.0.0.1"
//用户信息
typedef struct a{ 
    int fd;
	int size ;//每次发送文件的大小
    int flag;           //标志是登录还是注册
    int login;          //标志各种功能 2为登录成功 其他看menu
    int power;          //权限
    char gr_name[20]; //群名称
    char txt[100];      //文件传输权限
    char number[10];    //账号
    char passwd[20];    //密码
    char object[10];    //聊天对象
    char pathname[100]; //文件名
    char buf[10000];   //输入
}user;
 
//消息管理
typedef struct b{
    int flag;   //标志是哪一种消息请求
    int fd;
    char buf[MAXLEN];
    struct b *next;
}news;
char g_name[100];
int chatting;
int chat_flag = 1;
char name[100];
int file_size( char *filename );
void s();
void save_wenjian();
void ask();
void send_file();
void group_chat();
void del_friend();
void person_chat();
int log_in();
void set_in();
int denglu();
int menu();
void *request();
void baocun( user *guy );
void xiaoxi();

pthread_t tid;
int s_fd;
user guy;
news *head,*p1,*p2; 
char number[50];      //备份账号

int main()
{
    signal( SIGPIPE,SIG_IGN );
    struct sockaddr_in sin;
    int n,choose,ret;
    
    memset( &guy,0,sizeof(guy) ); 
    memset( &sin,0,sizeof(sin) );
    
    head = (news *)malloc( sizeof(news) );  //初始化头指针
    head->next = NULL;
    p1 = head;
    p2 = head;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    inet_aton(IP,&sin.sin_addr);

     if( (s_fd = socket(AF_INET,SOCK_STREAM,0)) < 0 )   //建立套接字
    {
        printf( "socket error\n" );
        exit(0);
    }
    
    if( connect(s_fd,(struct sockaddr *)&sin,sizeof(sin)) )     //连接
    {
        printf( "connect error\n" );
        exit(0);
    }
    
    ret = denglu();  //登录

    strcpy( number,guy.number );

    if( ret == 0 )
    {
        return 0;
    }

    pthread_create( &tid,NULL,(void *)request,NULL );

    while( choose = menu() )    //主菜单，各种功能
    {
        memset( guy.number,0,sizeof(guy.number) );
        strcpy( guy.number,number );
        switch(choose)
        {
            case 0:
                break;
            case 1: //添加好友
            {
                printf( "\n\t\t请输入添加账号：" );
                scanf( "%s",guy.object );       //输入要添加的账号
                if( strcmp(guy.object,guy.number) == 0 )
                {
                    printf( "\n\t\t帐号错误！\n" );
                    break;
                }
                guy.login = 1;
                if( send( s_fd,(void *)&guy,sizeof(guy),0 ) < 0 )
                {
                    printf( "case 1 send error\n" );
                    exit(0);
                }
                break;
            }
            case 11:
                system("clear");
                break ;
            case 2:         //好友列表
            {
                guy.login = 22;
                memset(guy.number,0,sizeof(guy.number));
                strcpy(guy.number,number);
                send( s_fd,(void *)&guy,sizeof(guy),0 );
                break;
            }
            case 3:         //私聊
            {
                person_chat();
                break;
            }
            case 4:         //群聊
            {
                group_chat();
                break;
            }
            case 5:   //消息管理
            {
                xiaoxi();
                break;
            }
            case 6:        //删除好友
            {
                del_friend();
                break;
            }
            case 7:         //查看聊天记录
            {
                guy.login = 8;
                send( s_fd,(void *)&guy,sizeof(guy),0 );
                break;
            }
            case 8:         //文件传输
            {
                char ch[10];
				printf("\n\t\t输入0退出...\n");
  				printf("\n\t\t--------------------------------------------------------------\n");

				printf("\n\t\t>>> 1.发送请求\n");
				printf("\n\t\t>>> 2.开始发文件\n");
  				printf("\n\t\t--------------------------------------------------------------\n");
				printf("\n\t\t请输入选项:");
				scanf( "%s",ch );
                while('\n'!=getchar());
                while( strcmp(ch,"1")!=0 && strcmp(ch,"2")!=0 && strcmp(ch,"0")!=0 )
                {
                    printf( "\n\t\t输入错误!\n" );
					printf("\n\t\t请重新输入:");
                    scanf( "%s",ch );
                } 
                if( strcmp(ch,"0") == 0 )
                {
                    break;
                }
                if( strcmp( ch,"1" ) == 0 )
                {
                    ask();
                }
                if( strcmp(ch,"2") == 0 )
                {
                    send_file();
                }
                break;
            }
        }
    }
    return 0;
}

int menu()      //主界面
{
    int ch;
    char n[100];
		
		printf("\n\t\t\t\t欢迎进入聊天室    %s",name);
  		printf("\n\t\t--------------------------------------------------------------\n");

        printf( "\n\t\t\t\t1.添加好友\n" );

        printf( "\n\t\t\t\t2.打开好友列表\n" );

        printf( "\n\t\t\t\t3.发起私聊\n" );

        printf( "\n\t\t\t\t4.群聊管理\n" );

        printf( "\n\t\t\t\t5.消息管理中心\n" );
		
        printf( "\n\t\t\t\t6.删除好友\n" );

        printf( "\n\t\t\t\t7.查看聊天记录\n" );

        printf( "\n\t\t\t\t8.文件传输\n\n" );
        printf( "\n\t\t\t\t按(0)退出聊天室 +_+   (11)刷新界面\n" );

        printf( "\n\t\t---------------------------------------------------------------\n" );
		printf("\n\t\t请输入选项:");

        scanf( "%s",n );
       while(strcmp(n,"11")!=0&&strcmp(n,"1")!=0&&strcmp(n,"2")!=0&&strcmp(n,"3")!=0&&strcmp(n,"4")!=0&&strcmp(n,"5")!=0&&strcmp(n,"6")!=0&&strcmp(n,"7")!=0&&strcmp(n,"8")!=0&&strcmp(n,"0")!=0)
       {
           printf( "\n\t\t错误选项，重新选择\n" );
		   printf("\n\t\t请重新输入:");
           scanf( "%s",n );
       }
        ch = atoi(n);
   return ch;
}

int log_in()   //登录
{
    int flag;
    guy.flag = 1;
    guy.login = 2;
    char *tmp;

    printf( "\n\t\t请输入账号：" );
    scanf( "%s",guy.number );
	strcpy(name , guy.number);
    tmp = getpass( "\n\t\t请输入密码:" );
    strcpy( guy.passwd,tmp );    

    if( send(s_fd,(void *)&guy,sizeof(user),0) < 0 )         //发送
    {
        printf( "\n\t\t发送出错\n" );
        exit(0);
    }
    
    if( recv(s_fd,(void *)&flag,sizeof(flag),0) < 0 )       //接收
    {
        printf( "\n\t\t接收出错\n" );
        exit(0);
    }
    if( flag == 1 )
    {
        printf( "\n\t\t登陆成功\n" );
        return 1;
    }
    else{
		printf("%d",flag);
		printf("\n\t\t登陆失败!\n");
		return 0 ;
	}
}

void set_in()       //注册
{
    int flag;
    guy.flag = 2;
    guy.login = 2;

    char passwd[20];
    printf( "\n\t\t请输入账号：" );
    scanf( "%s",guy.number );
	getchar();
    printf( "\n\t\t请输入密码：" );
    scanf( "%s",guy.passwd );
	getchar();
    printf( "\n\t\t请确认密码：" );
    scanf( "%s",passwd );
	getchar();
    if( (strcmp(passwd,guy.passwd)) == 0 )
    {
        if( send(s_fd,(void *)&guy,sizeof(user),0) < 0)      //发送
        {
            printf( "\n\t\t发送出错\n" );
            exit(0);
        }
        if( recv(s_fd,(void *)&flag,sizeof(flag),0) < 0 )    //接收
        {
            printf( "\n\t\t接收出错\n" );
            exit(0);
        }
       	
        if( flag == 1 )
        {
            printf( "\n\t\t注册成功\n" );
            return ;
        }
        else
        {
            if( flag == 0 )
                printf( "\n\t\t账号已存在 注册失败\n" );
            memset( &guy,0,sizeof(guy) );
            return ;
        }
    }
    else
    {
        printf( "\n\t\t两次密码不同\n" );
        return ;
    }

}



int denglu()             //登录界面
{
    char ch[50];
    while(1)
    {
        int n=0;
      printf("\n\t\t-----------------------------------------------------\n");
	printf("\n\t\t\t\t >>>  1.用户登陆\n");
	printf("\n\t\t\t\t >>>  2.用户注册\n");
	printf("\n\t\t\t\t >>>  0.退出程序  +_+\n");
  	printf("\n\t\t-----------------------------------------------------\n");
	printf("\n\t\t请输入选项:");
      scanf( "%s",ch );
      getchar();
        while( strcmp(ch,"1") != 0 && strcmp( ch,"2" ) != 0 && strcmp( ch,"0" ) != 0 )
        {
            printf( "\n\t\t错误选项，重新选择\n" );
			printf("\n\t\t请重新输入:");
            scanf( "%s",ch );
        }
        if( strcmp(ch,"1") == 0 )   //登录
        {
            n = log_in();
            if( n == 1 )
            {
                break;
            }
            else
                memset( &guy,0,sizeof(guy) );
        }

        if( strcmp(ch,"2") == 0 ) //注册
        {
            set_in();
        }
        if( strcmp( ch,"0" ) == 0 )
        {
            return 0;
        }
    }
    return 1;
}

void *request( void *arg )    //接收别的客户端发来的请求 添加好友 聊天什么的
{
    int ret;
    while(1)
    {
        if( (ret = recv( s_fd,(void *)&guy,sizeof(user),0 )) >0  )
        {
            memset( guy.number,0,sizeof(guy.number) );
            strcpy(guy.number,number);
            printf( "\n" );
            if( guy.login == 1 )  //有好友添加信息
            {
                printf( "\n\t\t有好友添加消息,进入消息中心查看并回复\n" );
                baocun( &guy );
            }
            if( guy.login == 11 )
            {
                printf( "\n\t\t好友添加消息\n" );
                printf( "%s\n",guy.buf );
            }
            if( guy.login == 111 )
            {
                printf( "%s\n",guy.buf );
            }
            if( guy.login == 22 )     //展示好友
            {
                printf( "\n\t\t你的好友有：\n" );
				printf("\n\t\t%s\n",guy.buf);
            }
            if( guy.login == 0 )   //有离线消息
            {
                printf("\n\t\t你有离线消息，去消息中心查看！\n");
                baocun( &guy );
            }
            if( guy.login == 3 )  //私聊消息
            {
                memset( guy.number,0,sizeof(guy.number) );
                strcpy( guy.number,number );

                if( chatting == 0 )
                {
                    printf( "\n\t\t有私聊消息,去消息中心查看！\n");
                    baocun( &guy );
                }
                if( chatting == 1 )     //如果在聊天就打印在屏幕上
                {
                    printf( "%s\n",guy.buf );
                    printf( "输入内容:\n" );
                }
            }

			if(guy.login == 42){
			
				printf("%s",guy.buf);
			}
			if(guy.login == 101){//当前用户已加入的群
			
				printf("\n\t\t你已经加入的群:\n");
				printf("%s",guy.buf);
			}
            if( guy.login == 43 )       //群聊消息
            {
				
                memset( guy.number,0,sizeof(guy.number) );
                strcpy( guy.number,number );
                if( chatting == 1 )     //正在聊天 就答应在屏幕上
                {
                    printf( "%s\n",guy.buf);
                    printf( "输入内容:\n" );
                }
                else
                {

                    printf( "\n\t\t你有来自%s群的消息，快去消息中心查看\n",guy.gr_name);
                    baocun(&guy);
                }
            }
			if(guy.login == 44){//邀请好友进群
				
				printf("%s",guy.buf);

			}
            if(guy.login == 80){
					
				printf("\n\t\t你已创建的群组:\n");
				printf("%s",guy.buf);
			}
            if( guy.login == 45 )       //展示群成员
            {
				printf("\n\t\t群%s\n",guy.object);
         		printf("\n\t\t%s\n",guy.buf);
			}
//yes
            if( guy.login == 8 )    //查看聊天记录
            {
                printf( "\n\t\t%s\n",guy.buf );
            }
            if( guy.login == 9 )    //文件传输的请求
            {
                printf( "\n\t\t有好友传文件给你，去消息中心回复\n" );
                baocun( &guy );
            }
            if( guy.login == 99 )   //请求的回复
            {
				if(strcmp(guy.buf , "y") == 0){
					printf("\n\t\t对方同意了你传文件给他,可以传文件了\n");
				}

				else{
				
					printf("\n\t\t对方拒绝你传文件给他\n");
				}
                if( strcmp(guy.buf,"y") == 0 )
                {
                    char tmp[100] = {0};
                    strcpy(tmp,guy.object);
                    strcat(tmp,guy.buf);
                    strcpy(guy.txt,tmp);
                    
                    baocun(&guy);
                }
            }
            if( guy.login == 999 )  //保存接的文件
            {
                save_wenjian();
            }
            
            if( guy.login == 123 || guy.login == 456 )      //上下线提醒
            {
                printf( "\n\t\t%s\n",guy.buf );
            }
        }
    }
}

void baocun( user *guy )   //保存服务器发来的信息
{
    p1 = (news *)malloc( sizeof(news) );
    p2->next = p1;
    p1->flag = guy->login;
    p1->fd = guy->fd;
    strcpy( p1->buf,guy->buf );
    p2 = p1;
    p1->next = NULL;
    p2->next = NULL;
}

void xiaoxi()   //在主线程处理服务器发来的消息
{
    int t;
    char c[20];
    news *p ;
    while(1)
    {
  		printf("\n\t\t-----------------------------------------------------\n");
		printf("\n\t\t\t\t>>>   1.添加好友请求\n");
		printf("\n\t\t\t\t>>>   2.私聊消息\n");
		printf("\n\t\t\t\t>>>   3.群聊消息\n");
		printf("\n\t\t\t\t>>>   4.离线消息\n");
		printf("\n\t\t\t\t>>>   5.传文件回复\n\n");
        printf("\n\t\t\t按0退出+_+      (11)刷新界面\n");
  		printf("\n\t\t-----------------------------------------------------\n");
		printf("\n\t\t请输入选项：");
        scanf( "%s",c );
        getchar();
        while( strcmp(c,"11")!=0&&strcmp(c,"1")!=0 && strcmp(c,"2")!=0 && strcmp(c,"3")!=0 && strcmp(c,"4")!=0 && strcmp(c,"0")!=0 && strcmp(c,"5")!=0 )
        {
            printf( "\n\t\t错误选项\n" );
			printf("\n\t\t请重新输入:");
            scanf( "%s",c );
            getchar();
        }
        
        if( strcmp(c,"0") == 0 )   //按零退出
        { 
            break;
        }
        if(strcmp(c ,"11")== 0){
        
            system("clear");
            break ;
        }
        if( strcmp(c,"1") == 0 )   //处理好友添加的请求
        {
            int t,flag=0;
            char ch[50];
            p = head->next;
            while( p)
            {
                if( p->flag == 1 )
                {
                    printf( "\n\t\t%s(%d)\n",p->buf,p->fd );
                }
                p = p->next;
            }
            printf( "\n\t\t请输入选项:" );
            scanf( "%s",ch );
            getchar();
            t = atoi(ch);
            if( t == 0 )
            {
                return ;
            }
            p = head->next;
            while( p )
            {
                if( p->fd == t )
                {
                    flag = 1;
                    break;
                }
                p = p->next;
            }
            if( flag == 0 )
            {
                printf( "\n\t\t暂时没有添加消息\n" );
                return ;
            }
            printf( "\n\t\t%s (y/n)\n",p->buf );
            memset(guy.buf,0,sizeof(guy.buf));
			printf("\n\t\t请输入:");
            scanf( "%s",guy.buf );
            guy.fd = p->fd; 
            guy.login = 11;
            send( s_fd,(void *)&guy,sizeof(user),0 );
        }
        
        if( strcmp(c,"2") == 0 )        //私聊消息
        {
            p = head->next;
            while( p )
            {
                if( p->flag == 3 )
                {
                    printf( "%s\n",p->buf );
                }
                p = p->next;
            }
            break;
        }

        if( strcmp(c,"3") == 0 )    //群聊消息
        {
            p = head->next;
            while( p )
            {
                if( p->flag == 43 )
                {
                    printf( "%s\n",p->buf);
                }
                p = p->next;
            }
            break;
        }

        if( strcmp(c,"4") == 0 )  //离线消息
        {
            p = head->next;
            while( p )
            {
                if( p->flag == 0 )
                    printf( "\n\t\t%s\n",p->buf );
                p = p->next;
            }
            break;
        }
        if( strcmp(c,"5") == 0 )    //文件传输请求回复
        {
            guy.login = 99;

            p = head->next;
            while( p )
            {
                if( p->flag == 9 )
                {
                    printf( "\n\t\t%s输入'y'或'n'\n",p->buf );
                    memset(guy.buf,0,sizeof(guy.buf));
					printf("\n\t\t你的选择：");
                    scanf( "%s",guy.buf );
                    getchar();
                    guy.fd = p->fd;
                    send( s_fd,(void *)&guy,sizeof(guy),0 );
                }
                p = p->next;
            }
        }
    }
}

void s( int sig )
{
    chat_flag = 0;
}

void person_chat()  //私聊
{
    guy.login = 3;
    chatting = 1;
    chat_flag = 1;
    
    char object[10] = {0};

    printf( "输入聊天对象的账号:" );
    scanf( "%s",guy.object );
    getchar();
    strcpy( object,guy.object );
	system("clear");
	printf("------------------------------------------------------\n");
	printf("\n\t\t输入n退出私聊.....+_+\n");
    while( chat_flag )
    {
        signal( SIGINT,s );
        if( chat_flag == 0 )
        {
            break;
        }
        printf( "输入内容:\n" );
        fgets( guy.buf,MAXLEN,stdin );
        strcpy( guy.object,object );
		if(!strcmp(guy.buf, "n\n"))break;
        send( s_fd,(void *)&guy,sizeof(user),0 );        
    }
    chatting = 0;
    chat_flag = 1;
    signal( SIGINT,SIG_DFL );
}

void del_friend()     //删除好友
{
    char ch;
    guy.login = 7;
    printf( "\n\t\t输入删除好友账号:" );
    scanf( "%s",guy.buf );
    getchar();
    
    printf( "\n\t\t你确定删除该好友！？(y/n)\n" );
    ch = getchar();
    if( ch == 'n' )
    {
        return ;
    }
    if( ch == 'y' )
    {
        send( s_fd,(void *)&guy,sizeof(guy),0 );
    }
}
void group_chat()       //群聊
{
    char object[10];
    char n[50];
    while(1){
    	printf("\n\t\t-----------------------------------------------------\n");
	    printf("\n\t\t\t\t>>>  1.已创建的群\n");
    	printf("\n\t\t\t\t>>>  2.创建群\n");
    	printf("\n\t\t\t\t>>>  3.群聊\n");
    	printf("\n\t\t\t\t>>>  4.邀请人进群\n");
    	printf("\n\t\t\t\t>>>  5.查看群成员\n");
    	printf("\n\t\t\t\t>>>  6.解散群\n");
    	printf("\n\t\t\t\t>>>  7.已加入的群\n\n");
    	printf("\n\t\t输入(0)退出+_+       (11)刷新界面....\n");
    	printf("\n\t\t-----------------------------------------------------\n");
    	printf("\n\t\t请输入选项：");	    
         scanf( "%s",n );
        while(strcmp(n,"11")!=0&&strcmp(n,"2")!=0 && strcmp(n,"3") != 0 && strcmp(n,"4")!=0&&strcmp(n,"0")!=0&&strcmp(n,"5")!=0&&strcmp(n,"6")!=0&&strcmp(n ,"1")!= 0&&strcmp(n ,"7")!= 0)
     {
            printf( "\n\t\t错误选项\n" );
		    printf("\n\t\t请重新输入:");
             scanf( "%s",n );
    }
         if( strcmp(n,"0") == 0 )
    {
        return ;
    }
	if(strcmp(n , "1") == 0){//展示群列表
	
		strcpy(guy.number , name);
		guy.login = 80 ;
		send(s_fd , (void *)& guy , sizeof(guy) , 0);
	}
    if(strcmp(n,"11")){
        system("clear");
    }
    if( strcmp(n,"2") == 0 ) //创建群
    {
        guy.login = 42;
        printf( "\n\t\t输入群账号:" );
        scanf( "%s",guy.buf );
        getchar();
		strcpy(guy.number,name);//将创建人传进
        guy.power = 1;
        send( s_fd,(void *)&guy,sizeof(guy),0 );
    }
    if( strcmp( n,"3" ) == 0 )      //群聊
    {
        guy.login = 43;
        printf( "\n\t\t输入群账号:" );
        scanf( "%s",guy.object );
        getchar();
        strcpy(guy.gr_name ,guy.object);
        strcpy(object,guy.object);
		system("clear");
		printf("\n----------------------------------------------------\n");
        printf( "\n\t\t输入n退出群聊...+_+\n" );
        while( chat_flag )
        {
            signal( SIGINT,s );
            if( chat_flag == 0 )
            {
                break;
            }
            chatting = 1;
            printf( "输入内容:\n" );
	
            fgets( guy.buf,4096,stdin );
            strcpy( guy.object,object );

			if(!strcmp(guy.buf ,"n\n"))break ;

            send( s_fd,(void *)&guy,sizeof(guy),0);
        }
        chatting = 0;
        chat_flag = 1;
    }
    if( strcmp( n,"4" ) == 0 )  //邀请人进群
    {

        guy.login = 44;
        printf( "\n\t\t邀进的群账号:" );
        scanf( "%s",guy.buf );
        getchar();
        printf( "\n\t\t邀请的好友账号:" );
        scanf( "%s",guy.object );
        getchar();
		strcpy(guy.number , name);
        send( s_fd,(void *)&guy,sizeof(guy),0 );
    }
    if( strcmp(n,"5") == 0 )    //查看群成员
    {
        guy.login = 45;
        printf( "\n\t\t请输入群账号:" );
        scanf( "%s",guy.object );
        send( s_fd,(void *)&guy,sizeof(guy),0 );
    }
    if( strcmp(n,"6") == 0 )    //解散群
    {
        guy.login = 46;
        printf( "\n\t\t输入解散的群号:" );
        scanf( "%s",guy.object );
        send( s_fd,(void *)&guy,sizeof(guy),0 );
    }
	if(strcmp(n, "7") == 0){//当前用户已加入的群
	
		guy.login = 101 ;
		strcpy(guy.number , name);
		send(s_fd , (void *)&guy , sizeof(guy) , 0);
	}
    }
}


void ask()      //向对象发出请求传输文件
{
    guy.login = 9;
    char tmp[100] = {0};
    printf( "\n\t\t输入要传文件的好友账号:" );
    scanf( "%s",guy.object );
    getchar();
    
    strcpy( tmp,guy.number );
    strcat( tmp,"想给你传文件,你是否同意?" );
    strcpy( guy.buf,tmp );
    
    send( s_fd,(void *)&guy,sizeof(guy),0 );

}

void send_file()       //文件传输
{

    char tmp[100] = {0};
    char pathname[50];
      int fp;
    int size,sum,t;
    sum = 0,t = 0;
    strcpy( tmp,guy.object );
    strcat(tmp,"y");
    //yes
    if( strcmp(tmp,guy.txt) != 0 )
    {
        printf( "\n\t\t未取得对方同意\n" );
        return ;
    }

    printf( "\n\t\t输入传输的文件名:" );
    scanf( "%[^\n]",pathname );
    getchar();
    
    size = file_size( pathname );  //获取文件大小

    fp = open( pathname,O_RDONLY );

    guy.login = 999;
    strcpy(  guy.pathname,pathname );
    memset( guy.buf,0,sizeof(guy.buf) );
    while( 1 )
    {
        t=read( fp,guy.buf,1000);
        if(t == -1){
            printf("\n\t\t文件可能不存在！\n");
            return ;
        }
		guy.size = 1000 ;
		if(t < 1000){//如果t小于1000，则读到文件末尾了让对面接收实际长度文件内容
		
			guy.size = t;

		}
        sum += t;
        send( s_fd,(void *)&guy,sizeof(guy),0 );
        
        memset( guy.buf,0,sizeof(guy.buf) );
        if( sum == size  )  //读取完成 跳出循环
            break;
    }
    printf("\n\t\t传输完毕.......\n");
    getchar();
    printf( "\n" );
    close(fp);
}

//denglu
void save_wenjian()         //保存接受的文件
{
    int fp;
	char file[100];
    strcat(guy.pathname,"_");
   		
	fp = open( guy.pathname,O_WRONLY | O_CREAT | O_APPEND,S_IRUSR | S_IWUSR );

	write( fp,guy.buf,guy.size);
    close(fp);
}

 
 
int file_size(char* filename)   //获取文件大小  
{  
    struct stat statbuf;  
    stat(filename,&statbuf);  
    int size=statbuf.st_size;  
  
    return size;  
}
