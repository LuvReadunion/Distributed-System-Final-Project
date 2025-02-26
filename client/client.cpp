#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include <sstream>

#include<winsock2.h>	//用于通信
#include<ws2tcpip.h>	//用于检索ip地址的新函数和结构

#pragma comment(lib,"ws2_32.lib")
#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_IP "127.0.0.1"	// 服务器为本机
#define DEFAULT_PORT "52000"	//默认端口
#define DEFAULT_BUFLEN 512 		//字符缓冲区长度

#pragma warning(disable:4996)	//解决scanf、strcpy兼容
#pragma warning(disable:6031)	//解决scanf返回值无用
#pragma warning(disable:6387)	//解决fp可能为0

char ret[100] = { 0 };			//每次连接读取的数据

using std::string;
using std::cin;
using std::vector;
using std::stringstream;

const string root_dir = "root/";	//强制根目录，不可修改，保证安全性
string res_dir = "resource/";		//资源目录
string cache_dir = "cache/";		//缓存目录
string commit_dir = "to_commit";	//提交目录(注意这里不加/)

void disconnect(SOCKET& ConnectSocket);

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

//通用指令分析函数
string analyze(const char* acc, string& order) {		
	string accept(acc);
	if (accept.size() > 9 && accept.substr(0, 8) == "get_file") {
		order = "get_file";
		return root_dir + res_dir + accept.substr(9);
	}
	else if (accept.size() > 12 && accept.substr(0, 11) == "upload_file") {
		order = "upload_file";
		return root_dir + res_dir + accept.substr(12);
	}
	else if (accept.size() > 3 && accept.substr(0, 2) == "cd") {
		string ret = accept.substr(3);
		//必须是文件夹格式(如首位为空格则非法)
		if (ret.front() == ' ') {
			printf("文件夹格式非法！\n");
		}
		else {
			order = "cd";
			return ret;
		}
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
		return root_dir + cache_dir + accept.substr(12);
	}
	else if (accept.size() > 5 && accept.substr(0, 4) == "pull") {		//update_file简写
		order = "update_file";
		return root_dir + cache_dir + accept.substr(5);
	}
	else if (accept.size() > 5 && accept.substr(0, 6) == "commit") {
		order = "commit";
		return "commit";
	}
	else if (accept.size() > 3 && accept.substr(0, 4) == "push") {
		order = "push";
		return "push";
	}
	else if (accept.size() > 3 && accept.substr(0, 4) == "help") {
		order = "help";
		return "help";
	}
	else if (accept.size() > 3 && accept.substr(0, 4) == "exit") {
		order = "exit";
		return "exit";
	}
	order = "fail";
	return "analyzed false";
}

//初始化并连接服务端
int init_and_connect(SOCKET & ret_socket) {

	//先设置服务器默认IP地址和端口号
	string SERVER_IP = DEFAULT_IP;		//可通过文件设置的服务器IP
	string SERVER_PORT = DEFAULT_PORT;	//可通过文件设置的服务器端口号

	//尝试读取本地设置的服务器IP地址和端口
	FILE* fp;
	if ((fp = fopen("root/ipconfig.txt", "r")) != NULL) {
		//读取文件
		//第一行为IP地址
		//第二行为端口号
		char IP_s[20] = { };
		memset(IP_s, 0, sizeof(IP_s));
		char PORT_s[10] = { };
		memset(PORT_s, 0, sizeof(PORT_s));
		fscanf(fp, "服务器IP设置:%s\n服务器监听端口:%s", IP_s, PORT_s);
		string SERVER_IP_T(IP_s);
		string SERVER_PORT_T(PORT_s);
		//关闭文件
		fclose(fp);
		//简单检查一下是否合法
		if (SERVER_IP_T.find_first_of('.') > 0
			&& SERVER_IP_T.find_first_of('.') < 4
			&& SERVER_IP_T.find_last_of('.') > 4
			&& SERVER_IP_T.find_last_of('.') < 12
			&& atoi(SERVER_PORT_T.c_str()) > 20 ){
			//合法则采用
			SERVER_IP = SERVER_IP_T;
			SERVER_PORT = SERVER_PORT_T;
		}
	}

	memset(ret, 0, sizeof(ret));

	//初始化(与server相同)
	WSADATA wsaData;	// 定义一个结构体成员，存放的是 Windows Socket 初始化信息
	//Winsock进行初始化
	//调用 WSAStartup 函数以启动使用 WS2 _32.dll
	int iResult;		// 函数返回数据，用于检测对应函数执行是否失败
	//WSAStartup的 MAKEWORD (2，2) 参数发出对系统上 Winsock 版本2.2 的请求，并将传递的版本设置为调用方可以使用的最版本的 Windows 套接字支持
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);	// 启动命令，如果返回为 0 ，说明成功启动

	if (iResult != 0) {	// 返回不为 0 启动失败
		printf("初始化Winsock出错: %d\n", iResult);
		return 1;
	}

	//为客户端创建套接字
	struct addrinfo* result = NULL, * ptr = NULL, hints;

	// ZeroMemory 函数，将内存块的内容初始化为零
	ZeroMemory(&hints, sizeof(hints));
	// addrinfo 在 getaddrinfo() 调用中使用的结构
	hints.ai_family = AF_INET;			//AF_INET 用于指定 IPv4 地址族
	hints.ai_socktype = SOCK_STREAM;	// SOCK_STREAM 用于指定流套接字
	hints.ai_protocol = IPPROTO_TCP;	// IPPROTO_TCP 用于指定 tcp 协议
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(SERVER_IP.c_str(), SERVER_PORT.c_str(), &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo 失败: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	SOCKET ConnectSocket = INVALID_SOCKET;//创建套接字对象

	//尝试连接到返回的第一个地址。
	ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	//检查是否存在错误，以确保套接字为有效套接字。
	if (ConnectSocket == INVALID_SOCKET) {
		//WSAGetLastError返回与上次发生的错误相关联的错误号。
		printf("套接字错误: %ld\n", WSAGetLastError());
		//调用 freeaddrinfo 函数以释放由 getaddrinfo 函数为此地址信息分配的内存
		freeaddrinfo(result);
		WSACleanup();//用于终止 WS2 _ 32 DLL 的使用。
		return 1;
	}

	//连接到套接字
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		//调用getaddrinfo
		//尝试连接到一个地址，直到一个成功	
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		//检查是否存在错误，以确保套接字为有效套接字。
		if (ConnectSocket == INVALID_SOCKET) {
			//WSAGetLastError返回与上次发生的错误相关联的错误号。
			printf("socket failed with error: %ld\n", WSAGetLastError());
			//调用 freeaddrinfo 函数以释放由 getaddrinfo 函数为此地址信息分配的内存
			freeaddrinfo(result);
			WSACleanup();//用于终止 WS2 _ 32 DLL 的使用。
			return 1;
		}

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);			//关闭一个已存在的套接字。
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}
	//应该尝试getaddrinfo返回的下一个地址,如果连接调用失败。但对于这个简单的例子，我们只是释放资源。由getaddrinfo返回并打印一个错误消息
	freeaddrinfo(result);//释放由 getaddrinfo 函数为此地址信息分配的内存

	if (ConnectSocket == INVALID_SOCKET) {
		printf("无法连接到服务器！！\n");
		WSACleanup();
		return 1;
	}

	//回传得到的套接字
	ret_socket = ConnectSocket;
	return 0;
}

//发送消息
int cli_send(SOCKET& ConnectSocket, const char * sendinfo) {
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

//接收消息
int accept(SOCKET& ConnectSocket, char* buffer, unsigned int buffer_size) {
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

//接收文件(不包含文件的描述信息，直接接收文件实体)
void accept_file(SOCKET& ConnectSocket, string download_path, unsigned long file_size) {
	FILE* fp;
	fp = fopen(download_path.c_str(), "wb");
	unsigned long file_receive_num = 0;					//单词收到的字节数
	unsigned long file_left_num = file_size;			//剩余需要字节数
	char buffer[DEFAULT_BUFLEN] = { 0 };				//接收缓存
	while (file_left_num > 0) {							//文件接收循环
		unsigned long to_receive_num = file_left_num < DEFAULT_BUFLEN ? file_left_num : DEFAULT_BUFLEN;
		memset(buffer, 0, sizeof(buffer));				//清空缓存
		file_receive_num = accept(ConnectSocket, buffer, to_receive_num);
		if (file_receive_num < 0) {
			printf("接收错误！");
			return;
		}
		fwrite(buffer, 1, file_receive_num, fp);
		file_left_num -= file_receive_num;
	}
	fclose(fp);
	//接收完毕
	Sleep(100);
	printf("文件接收完毕。\n");
}

//断开连接
void disconnect(SOCKET& ConnectSocket) {
	Sleep(200);
	closesocket(ConnectSocket);
	WSACleanup();
}

int main() {

	//初始化并连接服务器
	SOCKET ConnectSocket = INVALID_SOCKET;	//创建套接字对象
	init_and_connect(ConnectSocket);
	
	string now_dir = "";		//当前所在远程目录

	while (1) {
		//输入指令提示符
		string echo = root_dir + now_dir;
		echo.back() = '>';
		printf("%s", echo.c_str());
		//输入指令
		char sendinfo[100] = { 0 };
		string sendinfo_s;
		getline(cin, sendinfo_s);
		strcpy(sendinfo, sendinfo_s.c_str());
		//分析指令
		string order;
		string ana_ret = analyze(sendinfo, order);
		//下载文件或修改文件
		if (order == "get_file" || order == "update_file") {
			char rec_buffer[100] = { 0 };
			bool is_find = false;
			//向服务器发送指令
			cli_send(ConnectSocket, sendinfo);
			//接收服务器回应，查看是否找到文件，若找到则记录文件大小
			accept(ConnectSocket, rec_buffer, sizeof(rec_buffer));
			string read_buf(rec_buffer);
			unsigned long file_size = 0;
			if (read_buf.size() > 24 && read_buf.substr(0, 23) == "find successfully, num:") {
				is_find = true;
				file_size = atoi(read_buf.substr(24).c_str());
			}
			else {			//未找到文件，打印错误
				printf("%s\n", rec_buffer);
			}
			//找到则开始接收文件主体
			//这里ana_ret储存的是下载文件的路径(本身也传递了文件名)
			if (is_find)
				accept_file(ConnectSocket, ana_ret, file_size);
		}
		//上传文件
		else if (order == "upload_file") {
			char rec_buffer[100] = { 0 };
			//在资源目录下查找该文件
			FILE* fp;
			if ((fp = fopen(ana_ret.c_str(), "rb")) == NULL) {
				printf("未查找到%s文件。\n", ana_ret.c_str());
				fclose(fp);
				break;
			}
			//向服务器发送上传指令
			cli_send(ConnectSocket, sendinfo);
			//等待并接收服务器回应
			bool can_upload = false;
			int acc_result = accept(ConnectSocket, rec_buffer, sizeof(rec_buffer));
			if (acc_result > 0 && string(rec_buffer) == "To uploading file.") {
				can_upload = true;
			}
			else {
				printf("当前无法上传。\n");
				fclose(fp);
				break;
			}
			//获取文件大小
			fseek(fp, 0, SEEK_END);
			int file_size = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			//发送文件大小提示
			sprintf(sendinfo, "%d", file_size);
			cli_send(ConnectSocket, sendinfo);
			Sleep(100);
			//发送文件实体
			char send_buffer[DEFAULT_BUFLEN] = { 0 };			//发送缓存
			int read_num = 0;
			while ((read_num = fread(send_buffer, sizeof(char), DEFAULT_BUFLEN, fp)) > 0) {
				int iSendResult = send(ConnectSocket, send_buffer, read_num, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("发送失败: %d\n", WSAGetLastError());
					closesocket(ConnectSocket);
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
		}
		//改变服务器当前所在目录
		else if (order == "cd") {
			char rec_buffer[100] = { 0 };
			//向服务器发送指令
			cli_send(ConnectSocket, sendinfo);
			//接收服务器回应，查看是否找到文件夹
			accept(ConnectSocket, rec_buffer, sizeof(rec_buffer));
			string read_buf(rec_buffer);
			if (read_buf.substr(0, 2) == "OK") {
				if (ana_ret == ".") {
					continue;
				}
				//切换成功不需要直接提示，改变输入指示提示符即可
				now_dir = read_buf.substr(3);
			}
			else {
				printf("%s", rec_buffer);
				//printf("切换文件夹失败，请确认是否有该文件夹。\n");
			}
		}
		//调用远程指令操作(服务器实现为调用cmd)
		else if (order == "remote_list"		//展示远程文件系统
			|| order == "remote_cmd"		//自定义调用远程cmd(也是这一系列指令的核心构建)
			|| order == "log_show"			//展示服务器端的日志文件
			|| order == "log_clear"			//清理服务器端的日志文件
			)
		{
			//向服务器发送指令
			cli_send(ConnectSocket, sendinfo);
			//等待并接收服务器回应
			char rec_buffer[10000] = { 0 };
			int acc_result = accept(ConnectSocket, rec_buffer, sizeof(rec_buffer));
			if (acc_result > 0 ) {
				printf("%s", rec_buffer);
			}
		}
		//提交本地对文件的修改(不向服务器发消息)
		else if (order == "commit") {
			char cmd[100] = { 0 };				//cmd指令
			//事先进入当前目录
			string temp = "cd " + root_dir + cache_dir + " & copy /Y .\\ ..\\" + commit_dir;
			strcpy(cmd, temp.c_str());			//cmd指令内容
			char result[1000] = { 0 };			//cmd指令调用返回结果
			if (exe_cmd(cmd, result)) {			//返回非0表示执行错误
				printf("cmd指令调用错误！\n");
			}
			else {								//执行无误，成功commit
				printf("%s", result);
			}
		}
		//推送已修改的文件
		else if (order == "push") {
			//先得到全部的处于to_commit中的文件
			vector<string> file_to_push;
			{
				char cmd[100] = { 0 };				//cmd指令
				//事先进入当前目录
				string temp = "cd " + root_dir + commit_dir + " & dir /b";
				strcpy(cmd, temp.c_str());			//cmd指令内容
				char result[1000] = { 0 };			//cmd指令调用返回结果
				int cmd_result = exe_cmd(cmd, result);	//cmd执行
				if (cmd_result) {						//如果调用返回值非0则说明cmd运行错误
					printf("cmd指令调用错误！\n");
				}
				else {									//找到对应对象
					//对结果进行分行(字符串分割)
					string file_s(result);
					string temp;
					stringstream in;
					in.str(file_s);
					while (getline(in, temp, '\n')) {
						file_to_push.push_back(temp);
					}
				}
			}
			//逐个文件进行传输(隐式调用upload_file)
			for (auto& c : file_to_push) {
				string path = root_dir + commit_dir + '/' + c;
				char rec_buffer[100] = { 0 };
				//在资源目录下查找该文件
				FILE* fp;
				if ((fp = fopen(path.c_str(), "rb")) == NULL) {
					printf("未查找到%s文件。\n", c.c_str());
					fclose(fp);
					continue;
				}
				//向服务器发送该文件的上传指令
				string upload_command = "upload_file " + c;
				char sendinfo_temp[100] = { 0 };
				strcpy(sendinfo_temp, upload_command.c_str());
				cli_send(ConnectSocket, sendinfo_temp);
				//等待并接收服务器回应
				bool can_upload = false;
				int acc_result = accept(ConnectSocket, rec_buffer, sizeof(rec_buffer));
				if (acc_result > 0 && string(rec_buffer) == "To uploading file.") {
					can_upload = true;
				}
				else {
					printf("当前无法上传。\n");
					fclose(fp);
					continue;
				}
				//获取文件大小
				fseek(fp, 0, SEEK_END);
				int file_size = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				//发送文件大小提示
				sprintf(sendinfo, "%d", file_size);
				cli_send(ConnectSocket, sendinfo);
				Sleep(10);
				//发送文件实体
				char send_buffer[DEFAULT_BUFLEN] = { 0 };			//发送缓存
				int read_num = 0;
				while ((read_num = fread(send_buffer, sizeof(char), DEFAULT_BUFLEN, fp)) > 0) {
					int iSendResult = send(ConnectSocket, send_buffer, read_num, 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("发送失败: %d\n", WSAGetLastError());
						closesocket(ConnectSocket);
						WSACleanup();
						return 1;
					}
					//printf("字节发送: %d\n", iSendResult);
				}
				//关闭文件流
				fclose(fp);
				//发送完毕
				Sleep(10);
				printf("文件%s发送完毕。\n", c.c_str());
			}
			//删除to_commit中的文件
			{
				char cmd[100] = { 0 };				//cmd指令
				//事先进入当前目录
				string temp = "cd " + root_dir + commit_dir + " & del /Q .";
				strcpy(cmd, temp.c_str());			//cmd指令内容
				char result[1000] = { 0 };			//cmd指令调用返回结果
				int cmd_result = exe_cmd(cmd, result);	//cmd执行
				if (cmd_result == 1) {					//如果调用返回值为1则说明cmd运行错误
					printf("cmd指令调用错误！\n");
				}
				else {									//找到对应对象
					printf("已自动将to_commit文件夹清空。\n");
				}
			}
		}
		//本地查看help文档(不向服务器发消息)
		else if (order == "help") {
			//打开help.txt文件
			FILE* fp;
			fp = fopen(string(root_dir + "help.txt").c_str(), "r");
			char buff[1000] = { 0 };
			memset(buff, 0, sizeof(buff));
			//读取文件
			fread(buff, sizeof(char), sizeof(buff), fp);
			//关闭日志文件
			fclose(fp);
			printf("%s", buff);
		}
		//退出连接
		else if (order == "exit") {
			//删除cache中的文件
			{
				char cmd[100] = { 0 };				//cmd指令
				//事先进入当前目录
				string temp = "cd " + root_dir + cache_dir + " & del /Q .";
				strcpy(cmd, temp.c_str());			//cmd指令内容
				char result[1000] = { 0 };			//cmd指令调用返回结果
				int cmd_result = exe_cmd(cmd, result);	//cmd执行
				if (cmd_result == 1) {					//如果调用返回值为1则说明cmd运行错误
					printf("cmd指令调用错误！\n");
				}
				else {									//找到对应对象
					printf("已自动将cache文件夹清空。\n");
				}
			}
			//向服务器发送断连提示，防止服务器一直等待
			strcpy(sendinfo, "exit");
			cli_send(ConnectSocket, sendinfo);
			//断开连接后退出
			disconnect(ConnectSocket);
			break;
		}
		//非法指令
		else {
			printf("非法指令！请重新输入。(使用“help”指令可以查看帮助文档。)\n");
		}
	}

	return 0;
}
