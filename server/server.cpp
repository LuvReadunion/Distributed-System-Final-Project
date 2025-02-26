#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<thread>
#include<iostream>
#include<vector>
#include<time.h>
#include<mutex>

#include<winsock2.h>	//用于通信
#include<ws2tcpip.h>	//用于检索ip地址的新函数和结构

#include "file_list.h"
#include<map>

#include "paxos.h"		//Paxos一致性算法

#pragma comment(lib,"ws2_32.lib")
#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_IP "127.0.0.1"	//服务器为本机
#define DEFAULT_PORT "52000"	//默认端口
#define DEFAULT_BUFLEN 512 		//字符缓冲区长度

#pragma warning(disable:4996)	//解决scanf、strcpy兼容
#pragma warning(disable:6031)	//解决scanf返回值无用
#pragma warning(disable:6387)	//解决fp可能为0
#pragma warning(disable:28183)	//解决fp可能为0

char ret[100] = { 0 };			//每次连接读取的数据

struct addrinfo* l_result = NULL, * ptr = NULL, hints;	//监听使用的地址簇协议

using std::string;
using std::cout;
using std::endl;
using std::thread;					//需要使用std里的thread
using std::vector;
using std::mutex;					//多个线程写同一个文件需要上锁
using std::map;
using std::pair;

mutex mx_log;						//日志文件的锁

const string root_dir = "root/";	//强制根目录，不可修改，保证安全性

//文件节点列表
map<string, mutex*> file_list;

//Server_Node列表
vector<Server_Node> server_list;

int main_init() {
	
	//读取file_list文件，建立一系列的file_node
	{
		file_list_read(root_dir, file_list);
		
		//尝试接入Paxos(这里以3号为代理为例)
		//server_list[2]._new_value = &file_list;
	}

	//先设置服务器默认端口号
	string SERVER_PORT = DEFAULT_PORT;	//可通过文件设置的服务器端口号

	//尝试读取本地设置的服务器端口
	FILE* fp;
	if ((fp = fopen("root/ipconfig.txt", "r")) != NULL) {
		//读取文件
		//只读取端口号
		char PORT_s[10] = { };
		memset(PORT_s, 0, sizeof(PORT_s));
		fscanf(fp, "服务器监听端口:%s", PORT_s);
		string SERVER_PORT_T(PORT_s);
		//关闭文件
		fclose(fp);
		//简单检查一下是否合法
		if (atoi(SERVER_PORT_T.c_str()) > 20) {
			//合法则采用
			SERVER_PORT = SERVER_PORT_T;
		}
	}

	static int flag = 1;		//有关初始化的部分只执行一次(第一次执行完后置零)
	memset(ret, 0, sizeof(ret));

	//初始化
	static WSADATA wsaData;	// 定义一个结构体成员，存放的是 Windows Socket 初始化信息
	//Winsock进行初始化
	//调用 WSAStartup 函数以启动使用 WS2 _32.dll
	int iResult = 0;		// 函数返回数据，用于检测对应函数执行是否失败
	if (flag) {
		//WSAStartup的 MAKEWORD (2，2) 参数发出对系统上 Winsock 版本2.2 的请求，并将传递的版本设置为调用方可以使用的最版本的 Windows 套接字支持
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);	// 启动命令，如果返回为 0 ，说明成功启动

		if (iResult != 0) {	// 返回不为 0 启动失败
			printf("初始化Winsock出错: %d\n", iResult);
			return 1;
		}
	}

	if (flag) {
		ZeroMemory(&hints, sizeof(hints));	// 将内存块的内容初始化为零
		hints.ai_family = AF_INET; 			// AF_INET 用于指定 IPv4 地址族
		hints.ai_socktype = SOCK_STREAM;	// SOCK_STREAM 用于指定流套接字
		hints.ai_protocol = IPPROTO_TCP;	// IPPROTO_TCP 用于指定 tcp 协议
		hints.ai_flags = AI_PASSIVE;		// 指定 getaddrinfo 函数中使用的选项的标志。AI_PASSIVE表示：套接字地址将在调用 bindfunction 时使用

		// 从本机中获取 ip 地址等信息为了 sockcet 使用
		// getaddrinfo 函数提供从 ANSI 主机名到地址的独立于协议的转换。
		// 参数1：该字符串包含一个主机(节点)名称或一个数字主机地址字符串。
		// 参数2：服务名或端口号。
		// 参数3：指向 addrinfo 结构的指针，该结构提供有关调用方支持的套接字类型的提示。
		// 参数4：指向一个或多个包含主机响应信息的 addrinfo 结构链表的指针。
		iResult = getaddrinfo(NULL, SERVER_PORT.c_str(), &hints, &l_result);
		if (iResult != 0) {
			printf("解析地址/端⼝失败: %d\n", iResult);
			WSACleanup();
			return 1;
		}
	}

	flag = 0;
	return 0;
}

SOCKET listening() {
	static int flag = 1;		//有关初始化的部分只执行一次(第一次执行完后置零)
	int iResult = 0;		// 函数返回数据，用于检测对应函数执行是否失败
	
	// 创建监听的socket对象，使服务器侦听客户端连接
	static SOCKET ListenSocket = INVALID_SOCKET;		//INVALID_SOCKET定义代表遮套接字无效
	// socket 函数创建绑定到特定
	//为服务器创建一个SOCKET来监听客户端连接
	//socket函数创建绑定到特定传输服务提供者的套接字。
	//参数1：地址族规范
	//参数2：新套接字的类型规范
	//参数3：使用的协议
	if (flag) {
		ListenSocket = socket(l_result->ai_family, l_result->ai_socktype, l_result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {	//检查是否有错误，以确保套接字为有效的套接字
			printf("套接字错误: %ld\n", WSAGetLastError());
			freeaddrinfo(l_result);	 //调用 freeaddrinfo 函数以释放由 getaddrinfo 函数为此地址信息分配的内存。
			WSACleanup();			//终止 WS2_32 DLL 的使用
			exit(0);
		}

		//绑定套接字
		//要使服务器接受客户端连接，必须将其绑定到系统中的网络地址。
		//Sockaddr结构保存有关地址族、IP 地址和端口号的信息。
		//bind函数将本地地址与套接字关联起来。设置TCP监听套接字
		//参数1：标识未绑定套接字的描述符。
		//2：一个指向本地地址sockaddr结构的指针，用于分配给绑定的套接字。这里面有Sockaddr结构
		//3：所指向值的长度(以字节为单位)
		iResult = bind(ListenSocket, l_result->ai_addr, (int)l_result->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("设置TCP监听套接字失败: %d\n", WSAGetLastError());
			freeaddrinfo(l_result);		// 调用 bind 函数后，不再需要地址信息 释放
			closesocket(ListenSocket);	// 关闭一个已存在的套接字
			WSACleanup();
			return 1;
		}

		//监听套接字

		//将套接字绑定到系统的ip地址和端口后，服务器必须在IP地址和端口上监听传入的连接请求
		//listen函数将套接字置于侦听传入连接的状态。
		//参数1：标识已绑定的未连接套接字的描述符。
		//2：挂起连接队列的最大长度。如果设置为SOMAXCONN，负责套接字的底层服务提供者将把待办事项设置为最大合理值
		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
			// SOMAXCONN定义了此套接字允许最大连接
			printf("监听传入失败: %ld\n", WSAGetLastError());
			closesocket(ListenSocket);	// 关闭一个已连接的套接字
			WSACleanup();
			return 1;
		}
	}

	flag = 0;
	//以上部分其实可以移至main_init函数部分，因为只需要一个ListenSocket
	
	//接收来自客户端的连接

	//当套接字监听连接后，程序必须处理套接字上的连接请求
	//创建临时套接字对象，以接受来自客户端的连接
	SOCKET ClientSocket;

	//通常，服务器应用程序将被设计为侦听来自多个客户端的连接。
	ClientSocket = INVALID_SOCKET;		//INVALID_SOCKET定义代表套接字无效
	//accept函数允许套接字上的传入连接尝试
	//参数1：一个描述符，用来标识一个套接字，该套接字使用listen函数处于侦听状态。连接实际上是用accept返回的套接字建立的。
	//2：一种可选的指向缓冲区的指针，用于接收通信层所知的连接实体的地址。addr参数的确切格式是由当socket来自so时建立的地址族决定的
	//3:一个可选的指针，指向一个整数，该整数包含addr参数所指向的结构的长度。
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("传入连接失败: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		exit(0);
	}
	return ClientSocket;
}

//通用指令分析函数
string analyze(const char* acc, string & order, string& dir) {
	string accept(acc);
	if (accept.size() > 9 && accept.substr(0, 8) == "get_file") {
		order = "get_file";
		return root_dir + dir + accept.substr(9);
	}
	else if (accept.size() > 12 && accept.substr(0, 11) == "upload_file") {
		order = "upload_file";
		return root_dir + dir + accept.substr(12);
	}
	else if (accept.size() > 3 && accept.substr(0, 2) == "cd") {
		order = "cd";
		return accept.substr(3);
	}
	else if (accept.size() > 10 && accept.substr(0, 11) == "remote_list") {
		order = "remote_list";
		return "remote_list";
	}
	else if (accept == "ls") {		//remote_list简写
		order = "remote_list";
		return "remote_list";
	}
	else if (accept.size() > 11 && accept.substr(0, 10) == "remote_cmd") {
		order = "remote_cmd";
		return accept.substr(11);
	}
	else if (accept.size() > 4 && accept.substr(0, 3) == "cmd") {		//remote_cmd简写
		order = "remote_cmd";
		return accept.substr(4);
	}
	else if (accept.size() > 7 && accept.substr(0, 8) == "log_show") {
		order = "log_show";
		return "log_show";
	}
	else if (accept.size() > 8 && accept.substr(0, 9) == "log_clear") {
		order = "log_clear";
		return "log_clear";
	}
	else if (accept.size() > 12 && accept.substr(0, 11) == "update_file") {
		//与get_file不一样的地方就是保存在cache中
		order = "update_file";
		return root_dir + dir + accept.substr(12);
	}
	else if (accept.size() > 5 && accept.substr(0, 4) == "pull") {		//update_file简写
		order = "update_file";
		return root_dir + dir + accept.substr(5);
	}
	else if (accept.size() > 3 && accept.substr(0, 4) == "exit") {
		order = "exit";
		return "exit";
	}
	order = "fail";
	return "analyzed false";
}

//发送消息(与客户端相同)
int ser_send(SOCKET& ConnectSocket, const char* sendinfo) {
	int iSendResult = send(ConnectSocket, sendinfo, (int)strlen(sendinfo), 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("发送失败: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);			//关闭套接字
		WSACleanup();
		return 1;
	}
	//printf("字节发送: %ld\n", iSendResult);
	return 0;
}

//接收消息(与客户端相同)
int ser_accept(SOCKET& ConnectSocket, char* buffer, unsigned long buffer_size) {
	int iResult = recv(ConnectSocket, buffer, buffer_size, 0);
	if (iResult > 0) {
		//printf("接收的字节数: %d\n", iResult);
	}
	else if (iResult == 0)
		printf("连接关闭...\n");
	else
		printf("连接失败！: %d\n", WSAGetLastError());
	return iResult;
}

//接收文件(与客户端相同)
int ser_accept_file(SOCKET& ConnectSocket, string download_path, unsigned long file_size) {
	//接收不需要上锁，但是如本来就有该文件记录则要先删除之
	if (file_list.find(download_path) != file_list.end()) {
		file_erase(download_path, file_list);
	}
	FILE* fp;
	fp = fopen(download_path.c_str(), "wb");
	unsigned long file_receive_num = 0;					//单词收到的字节数
	unsigned long file_left_num = file_size;			//剩余需要字节数
	char buffer[DEFAULT_BUFLEN] = { 0 };				//接收缓存
	while (file_left_num > 0) {							//文件接收循环
		unsigned long to_receive_num = file_left_num < DEFAULT_BUFLEN ? file_left_num : DEFAULT_BUFLEN;
		memset(buffer, 0, sizeof(buffer));				//清空缓存
		file_receive_num = ser_accept(ConnectSocket, buffer, to_receive_num);
		if (file_receive_num < 0) {
			printf("接收错误！");
			return 1;
		}
		fwrite(buffer, 1, file_receive_num, fp);
		file_left_num -= file_receive_num;
		//printf("%d\n", file_left_num);
	}
	fclose(fp);
	//接收完毕
	Sleep(10);
	printf("文件接收完毕。\n");
	//将其加入file_list
	file_add(download_path, file_list);
	return 0;
}

//断开连接(与客户端相同)
void disconnect(SOCKET& ConnectSocket) {
	Sleep(200);
	closesocket(ConnectSocket);
	WSACleanup();
}

//调用cmd指令
int exe_cmd(char cmd[], char* result) {
	char buffer[256];
	FILE* pipe = _popen(cmd, "r");		//打开管道，并执行指令
	if (!pipe)							//运行失败
		return 1;
	while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe))	//将管道输出到result中
			strcat(result, buffer);
	}
	_pclose(pipe);						//关闭管道
	//没有内容则说明指令有误，返回2
	if (strlen(result) == 0) {
		return 2;
	}
	return 0;
}

//更新日志
int log_update(const char* whole_order, string output, string client_ip) {		//日志更新(没有则产生)
	
	//先准备好需要写入的全部信息
	char log[1000] = { 0 };
	//获取当前时间字符串
	time_t current_time = time(NULL);
	strcat(log, ctime(&current_time));
	//写入完整指令
	strcat(log, whole_order);
	//写入输出(运行结果)
	strcat(log, output.c_str());
	//记录客户端IP地址
	strcat(log, client_ip.c_str());
	strcat(log, "\n");

	//上锁，防止多个线程同时写
	mx_log.lock();
	//打开日志文件
	FILE* fp;
	if ((fp = fopen("root/log.txt", "a")) == NULL) {
		printf("Warning: 日志记录出错。");
	}
	//log写入文件
	fwrite(log, 1, strlen(log), fp);
	//关闭日志文件
	fclose(fp);
	//解锁
	mx_log.unlock();

	return 0;
}

//打开并发送日志
int log_out(SOCKET & ClientSocket) {

	//上锁，防止多个线程同时访问
	mx_log.lock();
	//打开日志文件
	FILE* fp;
	if ((fp = fopen("root/log.txt", "rb")) == NULL) {
		printf("Warning: 日志获取出错。");
		mx_log.unlock();
		return 1;
	}
	//获取文件大小
	fseek(fp, 0, SEEK_END);
	unsigned long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	//太大的要重新发末尾的部分
	if (file_size > 9500) {
		fseek(fp, -9000, SEEK_END);
	}
	char send_buffer[10000] = { 0 };
	memset(send_buffer, 0, sizeof(send_buffer));
	//读取文件
	fread(send_buffer, sizeof(char), sizeof(send_buffer), fp);
	strcat(send_buffer, "\nLog End\n");
	//发送读取到的内容(如果太多就发送不了了，需要直接下载日志文件)
	ser_send(ClientSocket, send_buffer);
	//关闭日志文件
	fclose(fp);
	//解锁
	mx_log.unlock();
	printf("日志发送完毕。\n");
	return 0;
}

//清理日志
int log_clear(SOCKET& ClientSocket) {
	//上锁，防止多个线程同时访问
	mx_log.lock();
	//以"w"方式打开日志文件自动清空
	FILE* fp;
	if ((fp = fopen("root/log.txt", "w")) == NULL) {
		printf("Warning: 日志清理出错。");
		mx_log.unlock();
		return 1;
	}
	//关闭日志文件
	fclose(fp);
	//解锁
	mx_log.unlock();

	printf("日志清理完毕。\n");
	//清空成功则发送运行结果
	char sendinfo[50] = { 0 };
	strcpy(sendinfo, "日志清理完毕。\n");
	ser_send(ClientSocket, sendinfo);

	return 0;
}

//连接客户端的线程程序
int make_and_connect(SOCKET ClientSocket) {
	
	//通过SOCKET获取对方IP地址(写这个好折磨)
	struct sockaddr_in connectedAddr;
	int Add_len = sizeof(SOCKADDR_IN);			//初始化很重要不然会出错
	getpeername(ClientSocket, (struct sockaddr*)&connectedAddr, &Add_len);
	char ipAddr[INET_ADDRSTRLEN];//保存点分十进制的地址
	char aim_ip_port[50] = { 0 };
	sprintf(aim_ip_port, "%s:%d\n", inet_ntop(AF_INET, &connectedAddr.sin_addr, ipAddr, sizeof(ipAddr)), ntohs(connectedAddr.sin_port));
	//连接成功，写入日志
	log_update("Client connect: ", "", string(aim_ip_port));

	int iResult = 0;		// 函数返回数据，用于检测对应函数执行是否失败

	//在服务器上接受和发送数据
	char recvbuf[DEFAULT_BUFLEN]; 		//字符缓冲区数组(接收的存储数组)，只用来存放接收的指令
	int iSendResult;					//发送结果
	string ana_ret;						//指令分析结果
	char sendinfo[1000] = { 0 };		//发送的信息
	
	string now_dir = "";				//当前所在目录(默认为空，对应"root")

	while (1) {							//处理的主循环
		//接收之前需要清空接收的缓存
		memset(recvbuf, 0, sizeof(recvbuf));
		iResult = ser_accept(ClientSocket, recvbuf, DEFAULT_BUFLEN);
		if (iResult > 0) {
			
			//printf("接收的字节数: %d\n", iResult);
			//获得指令、处理后的文件名
			string order;
			//分析指令，ana_ret储存可能需要用到的文件目录或者cmd指令
			ana_ret = analyze(recvbuf, order, now_dir);
			
			//根据指令处理：
			//客户端下载文件
			if (order == "get_file" || order == "update_file") {
				if (ana_ret.empty())
					return 1;
				//在资源目录下查找该文件
				FILE* fp;
				bool is_find = true;
				if ((fp = fopen(ana_ret.c_str(), "rb")) == NULL) {
					printf("未查找到%s文件。\n", ana_ret.c_str());
					//发送未找到提示
					strcpy(sendinfo, "From Server: File is not found!");
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [File is not found.]\nClient:", string(aim_ip_port));
					//标记为失败
					is_find = false;
				}
				//找到文件后
				if (is_find) {
					//先对该文件上锁
					if (file_list.find(ana_ret) != file_list.end()) {
						file_list[ana_ret]->lock();
					}
					//获取文件大小
					fseek(fp, 0, SEEK_END);
					unsigned long file_size = ftell(fp);
					fseek(fp, 0, SEEK_SET);
					//发送找到文件提示
					sprintf(sendinfo, "find successfully, num: %d", file_size);
					ser_send(ClientSocket, sendinfo);
					Sleep(100);
					//发送文件实体
					char send_buffer[DEFAULT_BUFLEN] = { 0 };			//发送缓存
					unsigned long read_num = 0;
					while ((read_num = fread(send_buffer, sizeof(char), DEFAULT_BUFLEN, fp)) > 0) {
						iSendResult = send(ClientSocket, send_buffer, read_num, 0);
						if (iSendResult == SOCKET_ERROR) {
							printf("发送失败: %d\n", WSAGetLastError());
							//写入日志
							log_update(recvbuf, " [Send fail.]\nClient:", string(aim_ip_port));
							closesocket(ClientSocket);
							WSACleanup();
							return 1;
						}
						//printf("字节发送: %d\n", iSendResult);
					}
					//关闭文件流
					fclose(fp);
					//发送完毕
					Sleep(100);
					printf("文件发送完毕。\n");
					//写入日志
					log_update(recvbuf, " [Success.]\nClient:", string(aim_ip_port));
					//对该文件解锁
					if (file_list.find(ana_ret) != file_list.end()) {
						file_list[ana_ret]->unlock();
					}
				}
			}
			//客户端上传文件
			else if (order == "upload_file") {
				//发送可上传提示
				strcpy(sendinfo, "To uploading file.");
				ser_send(ClientSocket, sendinfo);
				//接收文件的大小参数
				char file_size_buf[50] = { 0 };
				ser_accept(ClientSocket, file_size_buf, sizeof(file_size_buf));
				int file_size = atoi(file_size_buf);
				//找到则开始接收文件主体
				//这里ana_ret储存的是下载文件的路径(本身也传递了文件名)
				int acc_result = ser_accept_file(ClientSocket, ana_ret, file_size);
				//写入日志
				if(acc_result == 0)
					log_update(recvbuf, " [Success.]\nClient:", string(aim_ip_port));
				else
					log_update(recvbuf, " [Fail.]\nClient:", string(aim_ip_port));
			}
			//切换当前所在目录
			else if (order == "cd") {
				//“cd .”和“cd ..”特判
				if (ana_ret == ".") {		//不变
					//发送运行结果
					strcpy(sendinfo, string("OK " + now_dir).c_str());
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Success.]\nClient:", string(aim_ip_port));
					continue;
				}
				else if(ana_ret == ".."){	//回退到上一个文件夹
					//如本来就在根文件夹则报错返回
					if (now_dir == "") {
						//发送运行结果
						strcpy(sendinfo, "已在根文件夹！\n");
						ser_send(ClientSocket, sendinfo);
						//写入日志
						log_update(recvbuf, " [Fail.]\nClient:", string(aim_ip_port));
						continue;
					}
					else {
						//先删末尾的"/"
						now_dir.pop_back();
						//再找到最后一个"/"
						int last_ind = now_dir.find_last_of('/');
						//删除末尾
						now_dir = now_dir.substr(0, last_ind + 1);
						//发送运行结果
						strcpy(sendinfo, string("OK " + now_dir).c_str());
						ser_send(ClientSocket, sendinfo);
						//写入日志
						log_update(recvbuf, " [Success.]\nClient:", string(aim_ip_port));
						continue;
					}
				}
				//先确定是否存在该目录
				char cmd[100] = { 0 };				//cmd指令
				//事先进入当前目录
				string temp = "cd " + root_dir + now_dir + " & dir /b | findstr /X \"" + ana_ret + "\"";
				strcpy(cmd, temp.c_str());			//cmd指令内容
				char result[1000] = { 0 };			//cmd指令调用返回结果
				int cmd_result = exe_cmd(cmd, result);	//cmd执行
				if (cmd_result == 2) {					//如果调用返回值为2则说明未找到文件夹可能的匹配项
					printf("文件夹未找到！\n");
					//发送运行错误提示
					strcpy(sendinfo, "文件夹未找到！\n");
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Fail.]\nClient:", string(aim_ip_port));
					continue;
				}
				else if (cmd_result == 1) {				//如果调用返回值为1则说明cmd运行错误
					printf("cmd指令调用错误！\n");
					//发送运行错误提示
					strcpy(sendinfo, "From Server: cmd wrong!\n");
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Fail.]\nClient:", string(aim_ip_port));
					continue;
				}
				else {									//找到对应对象
					//查询成功则切换到对应目录
					now_dir = now_dir + ana_ret + '/';
					//发送运行结果
					strcpy(sendinfo, string("OK " + now_dir).c_str());
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Success.]\nClient:", string(aim_ip_port));
					continue;
				}
			}
			//显示服务端root目录
			else if (order == "remote_list") {
				char cmd[100] = { 0 };				//cmd指令
				//事先进入当前目录
				string temp = "cd " + root_dir + now_dir + " & dir /b";
				strcpy(cmd, temp.c_str());			//cmd指令内容
				char result[1000] = { 0 };			//cmd指令调用返回结果
				if (exe_cmd(cmd, result)) {			//如果调用返回值非0则说明cmd运行错误
					printf("cmd指令调用错误！\n");
					//发送运行错误提示
					strcpy(sendinfo, "From Server: cmd wrong!\n");
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Fail.]\nClient:", string(aim_ip_port));
					continue;
				}
				else {	//有该文件夹
					//查询成功则发送运行结果
					strcpy(sendinfo, result);
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Success.]\nClient:", string(aim_ip_port));
					continue;
				}
			}
			//运行cmd指令
			else if (order == "remote_cmd") {
				char cmd[100] = { 0 };				//cmd指令
				//事先进入当前目录
				ana_ret = "cd " + root_dir + now_dir + " & " + ana_ret;
				strcpy(cmd, ana_ret.c_str());		//cmd指令内容
				char result[1000] = { 0 };			//cmd指令调用返回结果
				if (exe_cmd(cmd, result)) {			//如果调用返回值非0则说明错误
					printf("cmd指令调用错误！\n");
					//发送运行错误提示
					strcpy(sendinfo, "From Server: cmd wrong!\n");
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Fail.]\nClient:", string(aim_ip_port));
				}
				else {
					//查询成功则发送运行结果
					strcpy(sendinfo, result);
					ser_send(ClientSocket, sendinfo);
					//写入日志
					log_update(recvbuf, " [Success.]\nClient:", string(aim_ip_port));
				}
			}
			//展示日志
			else if (order == "log_show") {
				log_out(ClientSocket);
			}
			//清理日志
			else if (order == "log_clear") {
				log_clear(ClientSocket);
			}
			//退出连接
			else if (order == "exit") {
				//客户端表示已执行完请求，断开服务器连接以结束
				Sleep(200);
				iResult = shutdown(ClientSocket, SD_SEND);
				if (iResult == SOCKET_ERROR) {
					printf("关闭失败: %d\n", WSAGetLastError());
					//写入日志
					log_update(recvbuf, " [Fail to exit.]\nClient:", string(aim_ip_port));
					closesocket(ClientSocket);
					WSACleanup();
					return 1;
				}
				printf("客户端连接正常结束。\n");
				//结束通信也写入日志
				log_update("Client disconnect: ", "", string(aim_ip_port));
				return 0;
			}
			//非法指令不做处理
			else {
				//写入日志
				log_update(recvbuf, " [Illegal command.]\nClient:", string(aim_ip_port));
				return 1;
			}
		}
		else if (iResult == 0)
			printf("连接关闭...\n");
		else {
			printf("接收消息失败: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}
	}

	return 1;
}

//产生心跳的单独线程
void heartbeat() {
	SID next_id = SID_MIN;
	while (true) {
		printf("\nNow: %d\n", next_id);
		//打印展示各个Server_Node的Value
		for (int i = SID_MIN; i <= SID_MAX; ++i) {
			printf("SID:%d, Value:%d  ", i, server_list[i - SID_MIN]._value._value);
			if (i % 3 == 0) {
				cout << endl;
			}
		}
		//产生心跳
		server_list[next_id - SID_MIN].process();
		//心跳间隔
		Sleep(100);
		//下一次产生心跳的SID
		next_id = (next_id + 1 - SID_MIN) % (SID_MAX - SID_MIN + 1) + SID_MIN;
	}
}

int main() {

	//线程池
	vector<thread> threads;

	//以下是Paxos
	/*
	//实例化各个Server_Node节点
	for (int i = SID_MIN; i <= SID_MAX; ++i) {
		server_list.emplace_back(i, &server_list);
	}
	//心跳线程
	threads.emplace_back(heartbeat);
	*/

	//主线程初始化
	main_init();
	
	//主循环
	while (1) {
		//打印主线程监听记录
		cout << "开始(继续)监听下一个客户端请求。" << endl;
		//监听客户端请求
		SOCKET ClientSocket = listening();
		//打印收到客户端访问的记录
		cout << "收到并应答一个客户端请求。" << endl;
		//创建一个新线程和套接字，建立连接并进行通信
		//直接在线程池中构造，防止成为局部变量在循环结束时销毁
		threads.emplace_back(make_and_connect, ClientSocket);
	}

	return 0;
}