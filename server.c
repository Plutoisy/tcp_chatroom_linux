#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define SERVER_PORT 5000
#define LISTEN_BACKLOG 50
#define BUF_SIZE 1024
#define QUIT_MARK "**"
#define FILE_MARK "file"
#define BUFFER_SIZE                8192//最大传输文件大小8kb
#define FILE_NAME_MAX_SIZE         512

// 在线用户信息结构体
typedef struct {
    char name[BUF_SIZE];
    int socket;
} OnlineUser;

OnlineUser online_users[100];    // 最多100人同时在线
int online_count = 0;            // 当前在线用户数

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;    // 互斥锁

//检查用户名是否重复的函数
int check_user_existence(char* message, OnlineUser* online_users) {
    int name_exist = 1;
    printf("check_user_existence!\n");
    for (int i = 0; i < 100; i++) {
        if (strcmp(message, online_users[i].name) == 0) {
            name_exist = 0;
        }
    }
    return name_exist;
}

//线程中处理客户端的函数
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    int name_exist = 0;
    char message[BUF_SIZE];
    char combined_message[2048];
    char login_succ[2048];
    char logout_succ[2048];
    int n = 0;
    // 接收客户端的昵称并将其保存到在线用户列表中
    while(name_exist == 0){
    n = recv(client_socket, message, BUF_SIZE, 0);
    if (n < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    message[n] = '\0';
    
    //检查昵称是否重复
    
    if(check_user_existence(message, online_users)){
        name_exist = 1;
    }
    else{
    send(client_socket, "昵称已存在", strlen("昵称已存在"), 0);
    }
    }
    
    send(client_socket, "注册成功", strlen("注册成功"), 0);
    pthread_mutex_lock(&mutex);    // 加锁

    OnlineUser user;
    strcpy(user.name, message);
    user.socket = client_socket;
    online_users[online_count++] = user;

    printf("[INFO] %s 已连接\n", user.name);
    //把登陆信息转发给其他客户端
    sprintf(login_succ, "[INFO] %s 已连接", user.name);
    for (int i = 0; i < online_count; i++) {
            if (online_users[i].socket != client_socket) {
                send(online_users[i].socket, login_succ, strlen(login_succ), 0);
            }
        }
    pthread_mutex_unlock(&mutex);    // 解锁

    // 循环接收客户端的消息，并将其转发给所有在线用户
    while (1) {
        n = recv(client_socket, message, BUF_SIZE, 0);
        if (n < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        message[n] = '\0';

        //用户输入**退出循环
        if (strcmp(message, QUIT_MARK) == 0) {
            break;
        }
        

        //用户输入%向服务器发送文件，服务器收到%开启以下代码
        if (message[0] == '%') {
            pthread_mutex_lock(&mutex);      // 加锁
            send(client_socket, "准备发送", strlen("准备发送"), 0);
            char buffer[BUFFER_SIZE];
            int length;
            bzero(buffer, sizeof(buffer));
            //接受客户端传来的文件名存到buffer中
            length = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (length < 0)
            {
                printf("服务器接收数据失败\n");
                break;
            }
            else if (strstr(buffer, "文件找不到") != NULL)
            {
                printf("文件找不到\n");
                
            }
            else
            {
                printf("服务器接收数据成功\n");
                char file_name[FILE_NAME_MAX_SIZE + 1];
                bzero(file_name, sizeof(file_name));
                strncpy(file_name, buffer,strlen(buffer) > FILE_NAME_MAX_SIZE ? FILE_NAME_MAX_SIZE : strlen(buffer));
                //新建名为file_name的文件，准备把buffer中的内容写入file_name
                FILE *fp = fopen(file_name, "w");  
                if (fp == NULL)  
                {  
                    printf("文件: %s 无法打开写入\n", file_name);  
                    exit(1);  
                }  
                // 从服务器端接收数据到buffer中   
                bzero(buffer, sizeof(buffer));  
                length = 0;  
                length = recv(client_socket, buffer, BUFFER_SIZE, 0);
                //printf("test\n");
                //printf("%s",buffer);
                //printf("%d\n",length);
                if (length < 0)  
                {  
                    printf("从服务器接收数据失败\n");  
                    break;  
                }  
                //写入数据到file_name
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
            pthread_mutex_unlock(&mutex);    // 解锁     
        }


        //用户输入^从服务器接收文件，服务器收到^开启以下代码
        if (message[0] == '^') 
        {
            pthread_mutex_lock(&mutex);    // 加锁
            //printf("test\n");
            send(client_socket, "准备接收", strlen("准备接收"), 0);
            char buffer[BUFFER_SIZE];
            int length;
            bzero(buffer, sizeof(buffer));
            //接受客户端传来的文件名存到buffer中
            length = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (length < 0)
            {
                printf("服务器接收数据失败\n");
                break;
            }
            else
                printf("服务器接收数据成功\n");
    
            char file_name[FILE_NAME_MAX_SIZE + 1];
            bzero(file_name, sizeof(file_name));
            strncpy(file_name, buffer,strlen(buffer) > FILE_NAME_MAX_SIZE ? FILE_NAME_MAX_SIZE : strlen(buffer));
            //在服务器中寻找名为file_name的文件
            FILE *fp = fopen(file_name, "r");  //获取文件操作符
            if (fp == NULL)
            {
                printf("文件:\t%s 找不到\n", file_name);
                send(client_socket, "文件找不到", strlen("文件找不到"), 0); 
            }
            else
            {
                bzero(buffer, BUFFER_SIZE);
                int file_block_length = 0;
                while( (file_block_length = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0)
                {
                    // 发送buffer中的字符串到客户端
                    
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
            pthread_mutex_unlock(&mutex);    // 解锁
        }


        //处理除了**、^、%以外的信息
        pthread_mutex_lock(&mutex);    // 加锁
        printf("%s: %s\n", user.name, message);
        sprintf(combined_message, "%s:%s", user.name, message);//把name和message粘起来成name:message，让返回客户端的信息前带有用户名
        //将服务器收到的信息转发给除了发送信息的客户端以外的其他所有客户端
        for (int i = 0; i < online_count; i++) {
            if (online_users[i].socket != client_socket) {
                send(online_users[i].socket, combined_message, strlen(combined_message), 0);
            }
        }
        pthread_mutex_unlock(&mutex);    // 解锁
    }

    // 客户端退出，从在线列表中删除并关闭连接
    pthread_mutex_lock(&mutex);    // 加锁

    printf("[INFO] %s 已退出\n", user.name);
    //把退出信息转发给其他客户端
    sprintf(logout_succ, "[INFO] %s 已退出", user.name);
    for (int i = 0; i < online_count; i++) {
            if (online_users[i].socket != client_socket) {
                send(online_users[i].socket, logout_succ, strlen(logout_succ), 0);
            }
        }
        
        
       //处理用户退出
       for (int i = 0; i < online_count; i++) {
        if (strcmp(online_users[i].name, user.name) == 0) {
            for (int j = i + 1; j < online_count; j++) {
                online_users[j - 1] = online_users[j];
            }
            online_count--;
            break;
        }
    }

    pthread_mutex_unlock(&mutex);    // 解锁

    close(client_socket);
    return NULL;
    
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t address_length = sizeof(client_address);
    pthread_t thread;

    // 创建服务器Socket对象
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址信息
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVER_PORT);

    // 绑定服务器地址和端口号
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 启动监听并设置最大等待连接数
    if (listen(server_socket, LISTEN_BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[INFO] 服务器已启动，监听端口 %d\n", SERVER_PORT);

    // 循环接受客户端连接请求
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &address_length);
        if (client_socket < 0) {
            perror("accept");
            break;
        }

        // 启动新线程处理客户端请求
        pthread_create(&thread, NULL, handle_client, (void *)&client_socket);
        pthread_detach(thread);
    }

    // 关闭服务器连接
    close(server_socket);

    return 0;
}

