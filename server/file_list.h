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

#pragma warning(disable:4996)	//���scanf��strcpy����

mutex mx_list;					//�ļ����ļ��������

string root_d = "";				//��¼һ��root_dir

//��ȡfile_list
void file_list_read(const string & root_dir, map<string, mutex*>& file_list) {
	//��file_list.txt�ļ�
	FILE* fp;
	fp = fopen(string(root_dir + "file_list.txt").c_str(), "r");
	if (fp == NULL) {
		printf("�ļ��б��ȡ���󣡷�������ʼ��ʧ�ܡ�\n");
		exit(0);
	}
	char buff[100] = { 0 };
	memset(buff, 0, sizeof(buff));
	//���ж�ȡ�ļ�
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		string read_buf(buff);
		read_buf.pop_back();
		file_list[read_buf] = new mutex;
	}
	fread(buff, sizeof(char), sizeof(buff), fp);
	//�ر���־�ļ�
	fclose(fp);
	//��¼һ��root_dir
	root_d = root_dir;
}

//�ļ��б�ӽڵ�
void file_add(const string& path, map<string, mutex*>& file_list) {
	file_list[path] = new mutex;
	//д��file_list
	//����
	mx_list.lock();
	//д��
	string temp(path);
	temp.push_back('\n');
	FILE* fp;
	fp = fopen(string(root_d + "file_list.txt").c_str(), "a");
	fputs(temp.c_str(), fp);
	fclose(fp);
	//����
	mx_list.unlock();
}

//�ļ��б�ɾ�ڵ�(��ȫɾ)
void file_erase(const string& path, map<string, mutex*>& file_list) {
	if (file_list.find(path) != file_list.end()) {
		delete file_list[path];
		file_list.erase(path);
		//����
		mx_list.lock();
		//����д������ֱ��ȫ����д����
		//д��
		FILE* fp;
		fp = fopen(string(root_d + "file_list.txt").c_str(), "w");
		for (auto& c : file_list) {
			fputs((c.first + '\n').c_str(), fp);
		}
		fclose(fp);
		//����
		mx_list.unlock();
	}
}



