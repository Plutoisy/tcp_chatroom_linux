#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>                         // for sockaddr_in  
#include <sys/types.h>                          // for socket  

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5000
#define BUF_SIZE 1024
#define QUIT_MARK "quit"
#define FILE_MARK "file"
#define BUFFER_SIZE                   8192//最大传输文件大小8kb
#define FILE_NAME_MAX_SIZE            512  


//接收服务端消息函数，放在单独线程中
void *handle_rev(void *arg) 
{
    int ret;
    char recv_buf[2048];
    int client_socket = *(int *)arg;
    static int str = 0;
    while(1)
        {
            ret = recv(client_socket, recv_buf, sizeof(recv_buf), 0);
            if (ret == -1) 
            {
                perror("recv");
                exit(1);
            } 
            else if (ret == 0) 
            {
                printf("server closed\n");
                exit(1);
            } 
            else if (strstr(recv_buf, "准备发送") != NULL) 
            {//收到"准备发送"后断开此线程，因为此线程的rev和接收文件buffer的rev有冲突
                recv_buf[ret] = '\0';
                printf("%s\n", recv_buf);
                str = *(int*)arg;
    	        str++;
    	        //printf("func1: %d,addr:%p\n",str,&str);
                pthread_exit((void*)&str);
            }
            else if (strstr(recv_buf, "准备接收") != NULL) 
            {//收到"准备接收"后断开此线程，因为此线程的rev和接收文件buffer的rev有冲突
                recv_buf[ret] = '\0';
                printf("%s\n", recv_buf);
                str = *(int*)arg;
    	        str++;
    	        //printf("func1: %d,addr:%p\n",str,&str);
                pthread_exit((void*)&str);
            }
            else 
            {
                recv_buf[ret] = '\0';
                printf("%s\n", recv_buf);
            }
       }
}

int main() {
    int client_socket;
    int ret = 0;
    int *tmp;
    char recv_buf[2048];
    struct sockaddr_in server_address;
    char message[BUF_SIZE];
    pthread_t thread;
    // 创建客户端Socket对象
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址信息
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr);

    // 连接服务器端
    
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // 客户端登录，输入昵称并向服务器发送
    do{
    printf("请输入你的昵称：");
    fgets(message, BUF_SIZE, stdin);
    message[strlen(message) - 1] = '\0';
    send(client_socket, message, strlen(message), 0);
    
    //接受服务端发来的注册成功或失败的信息
    ret = recv(client_socket, recv_buf, sizeof(recv_buf), 0);
    recv_buf[ret] = '\0';
    printf("%s\n", recv_buf);
    }
    while(ret==15);//用户名重复收到的ret为15,进入while循环反之退出while循环

    //建立新线程接受服务端数据
    pthread_create(&thread, NULL, handle_rev, (void *)&client_socket);

    
    // 循环读取用户输入并将其发送至服务器
    while (fgets(message, BUF_SIZE, stdin) != NULL) 
    {
        //以下为接受服务器文件代码输入^开始
        if(message[0] == '^')
        {
            message[strlen(message) - 1] = '\0';
            send(client_socket, message, strlen(message), 0);//发送^给服务器
            //发送^给服务器后服务器会返回准备接收，接受此消息的代码由上面的线程处理，并自动断开线程
            pthread_join(thread,(void*)&tmp);//断开线程验证
            //printf("tmp get :%d,addr:%p\n",*tmp,tmp);
            printf("请输入文件名：");
            char file_name[FILE_NAME_MAX_SIZE + 1];  
            scanf("%s", file_name);  
            char buffer[BUFFER_SIZE];//缓存区
            bzero(buffer, sizeof(buffer));  
            strncpy(buffer, file_name, strlen(file_name) > BUFFER_SIZE ? BUFFER_SIZE : strlen(file_name));  
            // 向服务器发送buffer中的数据，此时buffer中存放的是客户端需要接收的文件的名字  
            send(client_socket, buffer, BUFFER_SIZE, 0);  
            bzero(buffer, sizeof(buffer));  
            int length = 0; 
            //接收服务器返回的文件数据或者返回“文件找不到”这几个字
            length = recv(client_socket, buffer, BUFFER_SIZE, 0);
            //printf("test\n");
            //printf("%s",buffer);
            //printf("%d\n",length);
            if (length < 0)  
            {  
                printf("从服务器接收数据失败\n");  
                break;  
            }  
            //如果返回的是“文件找不到”这几个字
            else if (strstr(buffer, "文件找不到") != NULL)
            {
                printf("文件找不到\n");
                    
            }
            //如果返回的是文件数据
            else
            {
                //建立名为file_name的文件用来存发来的数据
                FILE *fp = fopen(file_name, "w");  
                if (fp == NULL)  
                {  
                    printf("文件: %s 无法打开写入\n", file_name);  
                    exit(1);  
                }  
                //写入发来的数据
                int write_length = fwrite(buffer, sizeof(char), length, fp);  
                if (write_length < length)  
                {  
                    printf("文件:\t%s 写入失败\n", file_name);  
                    break;  
                }  
                bzero(buffer, BUFFER_SIZE);  
                printf("从服务器成功接收到文件: %s \n", file_name);  
                fclose(fp); 
            }
            //重新建立新线程接受服务端数据
            pthread_create(&thread, NULL, handle_rev, (void *)&client_socket);    
        }
        
        
        //下面的quit代码没啥用，不会进入这个else if，quit在服务端实现了，可能上面把message发服务器时处理了一下message导致strcmp判断有误
        else if(strcmp(message, QUIT_MARK) == 0)
        {
            break;
        }
        //上面的quit代码没啥用，不会进入这个else if，quit在服务端实现了，可能上面把message发服务器时处理了一下message导致strcmp判断有误
        
        
        //以下为给服务器发送文件代码输入%开始
        else if(message[0] == '%')
        {
            message[strlen(message) - 1] = '\0';
            send(client_socket, message, strlen(message), 0);//发送%给服务器
            //发送%给服务器后服务器会返回准备发送，接受此消息的代码由上面的线程处理，并自动断开线程
            pthread_join(thread,(void*)&tmp);//断开线程验证
            //printf("tmp get :%d,addr:%p\n",*tmp,tmp);
            printf("请输入文件名：");
            char file_name[FILE_NAME_MAX_SIZE + 1];  
            scanf("%s", file_name);  
            char buffer[BUFFER_SIZE];//缓存区
            bzero(buffer, sizeof(buffer));  
            strncpy(buffer, file_name, strlen(file_name) > BUFFER_SIZE ? BUFFER_SIZE : strlen(file_name)); 
            //在客户端文件夹中寻找file_name文件
            FILE *fp = fopen(file_name, "r");  //获取文件操作符
            if (fp == NULL)
            {
                printf("文件:\t%s 找不到\n", file_name);  
                //如果在客户端文件夹找不到文件，给服务器返回"文件找不到"这几个字
                send(client_socket, "文件找不到", strlen("文件找不到"), 0);   
            }
            else
            {
                //如果找到了，将文件名发给服务器，现在buffer是file_name
                send(client_socket, buffer, BUFFER_SIZE, 0);
                bzero(buffer, BUFFER_SIZE);
                int file_block_length = 0;
                while( (file_block_length = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0)//存文件内容到buffer
                {
                    // 发送buffer中的内容也就是文件内容到服务器
                    if (send(client_socket, buffer, file_block_length, 0) < 0)
                    {
                        printf("发送文件:%s 失败\n", file_name);
                        break;
                    }
    
                    bzero(buffer, sizeof(buffer));
                }
                fclose(fp);
                printf("文件:%s 已传输完成\n", file_name);
            }
            //重新建立新线程接受服务端数据
           pthread_create(&thread, NULL, handle_rev, (void *)&client_socket);
            
        }
        //处理除了**、^、%以外的信息
        else
        {
        message[strlen(message) - 1] = '\0';
        send(client_socket, message, strlen(message), 0);
        }
        
     }
    
    // 关闭客户端连接
    close(client_socket);

    return 0;
}
