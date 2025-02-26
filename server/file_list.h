#ifndef FILE_LIST
#define FILE_LIST
#endif

#include<iostream>
#include<vector>
#include<mutex>
#include<map>

using std::string;
using std::vector;
using std::mutex;
using std::map;

#pragma warning(disable:4996)	//解决scanf、strcpy兼容

mutex mx_list;					//文件表文件本身的锁

string root_d = "";				//记录一下root_dir

//读取file_list
void file_list_read(const string & root_dir, map<string, mutex*>& file_list) {
	//打开file_list.txt文件
	FILE* fp;
	fp = fopen(string(root_dir + "file_list.txt").c_str(), "r");
	if (fp == NULL) {
		printf("文件列表读取错误！服务器初始化失败。\n");
		exit(0);
	}
	char buff[100] = { 0 };
	memset(buff, 0, sizeof(buff));
	//逐行读取文件
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		string read_buf(buff);
		read_buf.pop_back();
		file_list[read_buf] = new mutex;
	}
	fread(buff, sizeof(char), sizeof(buff), fp);
	//关闭日志文件
	fclose(fp);
	//记录一下root_dir
	root_d = root_dir;
}

//文件列表加节点
void file_add(const string& path, map<string, mutex*>& file_list) {
	file_list[path] = new mutex;
	//写入file_list
	//上锁
	mx_list.lock();
	//写入
	string temp(path);
	temp.push_back('\n');
	FILE* fp;
	fp = fopen(string(root_d + "file_list.txt").c_str(), "a");
	fputs(temp.c_str(), fp);
	fclose(fp);
	//解锁
	mx_list.unlock();
}

//文件列表删节点(安全删)
void file_erase(const string& path, map<string, mutex*>& file_list) {
	if (file_list.find(path) != file_list.end()) {
		delete file_list[path];
		file_list.erase(path);
		//上锁
		mx_list.lock();
		//不想写检索，直接全部重写即可
		//写入
		FILE* fp;
		fp = fopen(string(root_d + "file_list.txt").c_str(), "w");
		for (auto& c : file_list) {
			fputs((c.first + '\n').c_str(), fp);
		}
		fclose(fp);
		//解锁
		mx_list.unlock();
	}
}



