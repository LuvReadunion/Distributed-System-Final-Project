#ifndef PAXOS
#define PAXOS
#endif

#define NUM unsigned long long
#define NUM_NULL 0

#define VAL map<string, mutex*>*
#define VAL_NULL nullptr

#define SID int
#define SID_NULL 0
#define SID_MIN  1
#define SID_MAX  9
#define MAJORITY 5
#define LEARN_NUM 3

#define LEARNING_TIME 10

#include<iostream>
#include<ctime>
#include<queue>

using std::queue;

//��Ϣ����ö��
enum Message_Type {
	NONE,
	NEVER_MIND,
	PRO_INVALID,
	NEW_PROPOSAL,
	PRE_REQUEST,
	ACC_REQUEST,
	PRE_REPLY,
	ACC_REPLY,
	LEA_REQUEST
};

//Number��
struct Number {
public:
	Number() {		//number��ֵ��Ϊ����ʱ��ʱ��
		_number = time(0);
	}
	Number(NUM num) { _number = num; }
	Number(const Number &  num) { _number = num._number; }
	NUM get_number() { return _number; }
	void set_number(NUM num) { _number = num; }
	bool operator > (const Number & num) {
		return this->_number > num._number;
	}
	bool operator < (const Number& num) {
		return this->_number < num._number;
	}
	bool operator == (const Number& num) {
		return this->_number == num._number;
	}
	bool operator >= (const Number& num) {
		return this->_number >= num._number;
	}
	bool operator <= (const Number& num) {
		return this->_number <= num._number;
	}
	void operator = (const Number& num) {
		this->_number = num._number;
	}
public:
	NUM		_number;
};

//NUMBER��NULL����
static const Number NUMBER_NULL = Number(NUM_NULL);

//Value��
struct Value {
public:
	Value() : _value(VAL_NULL){ }
	Value(VAL val) : _value(val) { }
public:
	VAL		_value;
};

//Proposal��
struct Proposal {
public:
	Proposal() : _number(NUMBER_NULL) { _value = VAL_NULL; }
	Proposal(VAL val) :_value(val) { }
	Proposal(Value val) :_value(val) { }
public:
	Number	_number;
	Value	_value;
};

//Message��
struct Message {
public:
	Message() : _type(NONE), _proposal(), _number(NUMBER_NULL), _ack(false), _source(SID_NULL) { }
	Message(Message_Type type, Proposal proposal, Number num = NUMBER_NULL, bool ack = false, SID s = SID_NULL)
		: _type(type), _proposal(proposal), _number(num), _ack(ack), _source(s) { }
public:
	Message_Type	_type;
	Proposal		_proposal;
	Number			_number;
	bool			_ack;
	SID				_source;
};

class Proposer {
public:
	//���캯��
	Proposer(SID id) : _id(id), _number(NUMBER_NULL) { }
	//��Ϣ�Ĵ���
	Message process(Message& msg) {
		//���ж���Ϣ����
		switch (msg._type) {
			case NEW_PROPOSAL:	//�µ��᰸����Ҫ����һ�׶�����
				//��ȡ���᰸����(value)
				_proposal = msg._proposal;
				//����һ������_number��number����proposal��
				{
					Number new_num = Number();
					if (new_num < _number) {	//����number�Ĳ������ɲ�Ӧ�ó���ì��
						printf("����������ֲ�����number���ɵ������\n");
						exit(0);
					}
					else {
						_proposal._number = new_num;
						//��װ��Ϣ
						Message request(PRE_REQUEST, _proposal, NUMBER_NULL, true, _id);
						//�����ظ���ͳ��������
						pre_count = 0;
						//��Ϊ��Ч
						is_valid = true;
						return request;			//������Ϣ
					}
				}
			case PRE_REPLY:		//һ�׶�����Ļ�Ӧ
				//���жϸ���Ϣ�Ƿ� ack == true
				if (msg._ack) {
					//�ж��Ƿ���Ч����Ч������
					if (!is_valid) {
						//������֪�����number
						_number = msg._number;
						//��װ��Ϣ
						Message request(PRO_INVALID, _proposal, _number, true, _id);
						return request;			//������Ϣ
					}
					//���м��������ж��ǲ��Ǵﵽmajority����Ӧ
					++pre_count;
					//����ﵽ�˾Ϳ��Է�����һ�׶ε�����
					if (pre_count >= MAJORITY) {
						//��װ��Ϣ
						Message request(ACC_REQUEST, _proposal, NUMBER_NULL, true, _id);
						//�����ظ���ͳ��������(ȷ��ֻ����һ�������Ϣ)
						pre_count = 0;
						//���ܽ׶ε�ͳ��������
						acc_count = 0;
						return request;			//������Ϣ
					}
					else {		//��δ�ﵽ�㹻��������Ӧ
						//��װ��Ϣ
						Message request(NEVER_MIND, _proposal, NUMBER_NULL, true, _id);
						return request;			//������Ϣ
					}
				}
				else {		//����۲���Ϣ���ݣ���֪���µ�proposal��number
					//������ǰ�᰸
					is_valid = false;
					//������֪�����number
					_number = msg._number;
					//��װ��Ϣ
					Message request(PRO_INVALID, _proposal, _number, true, _id);
					return request;			//������Ϣ
				}
			case ACC_REPLY:		//���׶�����Ļ�Ӧ
				//���ж��᰸�Ƿ���Ч����Ч������ͨ��
				if (!is_valid) {
					//������֪�����number
					_number = msg._number;
					//��װ��Ϣ
					Message request(PRO_INVALID, _proposal, _number, true, _id);
					return request;			//������Ϣ
				}
				//���жϸ���Ϣ�Ƿ� ack == true
				if (msg._ack) {
					//����
					++acc_count;
					//��װ��Ϣ
					Message request(NEVER_MIND, _proposal, NUMBER_NULL, true, _id);
					return request;			//������Ϣ
				}
				else {	//˵��acceptor���յ�����number��proposal
					//������ǰ�᰸
					is_valid = false;
					//������֪�����number
					_number = msg._number;
					//��װ��Ϣ
					Message request(PRO_INVALID, _proposal, _number, true, _id);
					return request;			//������Ϣ
				}
		}
		//δ��⵽��Ӧ�����򷵻�NONE��Ϣ
		return Message(NONE, _proposal, _number, false, _id);
	}
public:
	//�����Ҫʵ��Multi_Paxos������Ҫ�������proposal��չ��һ������
	Proposal	_proposal;		//׼���ύ��proposal
	Number		_number;		//��ʶ��������number
	int			pre_count = 0;	//��ȷ��׼��������
	int			acc_count = 0;	//��ȷ�Ͻ��յ�����
	bool		is_valid;		//�᰸�Ƿ񱻷���
	SID			_id;			//��ʶ
};

class Acceptor {
public:
	//���캯��
	Acceptor(SID id) : _id(id), _number(NUMBER_NULL) { }
	
	//��Ϣ�Ĵ���
	Message process(Message & msg) {
		//���ж���Ϣ����
		switch (msg._type) {
			case PRE_REQUEST:	//һ�׶�����
				//�ж����proposal��number�ǲ��Ǵ��ڵ�ǰ��number
				if (msg._proposal._number > _number) {
					//���µ�ǰ��_number
					_number = msg._proposal._number;
					//����ظ���Message
					Message reply(PRE_REPLY, _proposal, NUMBER_NULL, true, _id);
					return reply;		//������Ϣ
				}
				else {	//���򷵻�ack = false
					//����ظ���Message
					Message reply(PRE_REPLY, _proposal, _number, false, _id);
					return reply;		//������Ϣ
				}
			case ACC_REQUEST:	//���׶�����
				//�ж����proposal��number�ǲ��ǵ��ڵ�ǰ��number
				if (msg._proposal._number == _number) {
					//���ܸ�proposal
					_proposal = msg._proposal;
					//���µ�ǰ��_number(�ⲽ������һ���Ż���Ҳ��ԭ�������е�һ�������)
					_number = msg._proposal._number;
					//����ظ���Message
					Message reply(ACC_REPLY, _proposal, _number, true, _id);
					return reply;		//������Ϣ
				}
				else {	//���򷵻�ack = false
					//����ظ���Message
					Message reply(ACC_REPLY, _proposal, _number, false, _id);
					return reply;		//������Ϣ
				}
		}
		//δ��⵽��Ӧ�����򷵻�NONE��Ϣ
		return Message(NONE, _proposal, _number, false, _id);
	}

public:
	Proposal	_proposal;	//�ѽ��ܵ����µ�proposal
	Number		_number;	//�ѻظ�������number
	SID			_id;		//��ʶ
};

class Learner {
public:
	Learner(SID id) : _id(id) { }
	//��Ϣ��ѧϰ
	vector<Message> process(Message& msg) {
		//�����ϢֻӦ����Learn����Ϣ
		if (msg._type == LEA_REQUEST) {
			//�Ƚ�number,������յ�number������ȡ֮�������
			vector<Message> ret;
			if (msg._proposal._number > _proposal._number) {
				//����proposal
				_proposal = msg._proposal;
				//����ѧϰ��Message
				//����Լ���Ŵ�1~LEARN_NUM��Learner������Ϣ(ע������_source�Ĳ��ָ�Ϊ���Ŀ��sid)
				for (int i = 1; i <= LEARN_NUM; ++i) {
					SID aim_id = (_id + i - SID_MIN) % (SID_MAX - SID_MIN + 1) + SID_MIN;
					ret.emplace_back(LEA_REQUEST, _proposal, msg._number, false, aim_id);
				}
			}
			return ret;			//������Ϣ
		}
		//δ��⵽��Ӧ�����򷵻�NONE��Ϣ
		return vector<Message>(1, Message(NONE, _proposal, NUMBER_NULL, false, _id));
	}
public:
	Proposal	_proposal;	//�ѽ��ܵ����µ�proposal
	SID			_id;		//��ʶ
};

class Server_Node {
public:
	Server_Node(SID id, vector<Server_Node>* ser_list)
		: _id(id), _proposer(id), _acceptor(id), _learner(id), server_list(ser_list) { }
	//����Ϣ��������ȫ���ڵ�
	void send_to_all(const Message& msg) {
		for (int i = SID_MIN; i <= SID_MAX; ++i) {
			if (i != _id) {
				(*server_list)[i - SID_MIN].Message_Queue.push(msg);
			}
		}
	}
	//����Ϣ��������ĳ���ڵ�
	void send_to_one(const Message& msg, SID aim_id) {
		(*server_list)[aim_id - SID_MIN].Message_Queue.push(msg);
	}
	//����������
	void process() {
		//�۲��Ƿ����µ�value����
		if (_new_value._value != _value._value) {
			//����һ��Proposal����Ϣ�������ص�Proposer
			Proposal new_proposal(_new_value);
			//��װ��Ϣ
			Message request(NEW_PROPOSAL, new_proposal, NUMBER_NULL, true, _id);
			//����Proposer����õ���Ϣ
			request = _proposer.process(request);
			//����Ϣ��������ȫ���ڵ�
			send_to_all(request);
		}
		//������Ϣ�����е���Ϣ
		while (!Message_Queue.empty()) {
			Message msg = Message_Queue.front();
			Message_Queue.pop();
			//debug��ʾ����ӡSID�Լ����յ��İ���Ϣ
			if(msg._type != 8)
				printf("SID:%d ���յ���Ϣ:type = %d num = %d,SID = %d, ack = %d, val = %d\n",
					_id, msg._type, msg._number, msg._source, msg._ack, msg._proposal._value);
			switch (msg._type) {
				//�ɺ��Ե���Ϣ
				case NEVER_MIND:
					break;
				//��ǰ�᰸ʧЧ
				case PRO_INVALID:
					//���������Ͳ���������(�������һ��ʱ����ط�)
					break;
				//�µ��᰸��������Ӧ���յ��������
				case NEW_PROPOSAL:
					break;
				//׼���׶������·Ÿ�Acceptor����Ӧ��Ϣת����Դ�ڵ�
				case PRE_REQUEST:
				{
					Message reply = _acceptor.process(msg);
					send_to_one(reply, msg._source);
					break;
				}
				//���ս׶������·Ÿ�Acceptor����Ӧ��Ϣת����Դ�ڵ�
				case ACC_REQUEST:
				{
					Message reply = _acceptor.process(msg);
					send_to_one(reply, msg._source);
					break;
				}
				//׼���׶εĻ�Ӧ���·Ÿ�Proposer
				case PRE_REPLY:
				{
					Message reply = _proposer.process(msg);
					//����õ�����ϢΪACC_REQUEST���Ϳ��Խ���Ⱥ��2�׶ε�Proposal��Ϣ
					if (reply._type == ACC_REQUEST) {
						send_to_all(reply);
					}
					//�����᰸��ʧЧ
					else if (reply._type == PRO_INVALID) {
						;//�������������
					}
					else {
						;//һ��ΪNEVER_MIND����������
					}
					break;
				}
				//���ܽ׶εĻ�Ӧ���·Ÿ�Proposer
				case ACC_REPLY:
				{
					Message reply = _proposer.process(msg);
					//�����᰸��ʧЧ
					if (reply._type == PRO_INVALID) {
						;//�������������
					}
					else {
						;//һ��ΪNEVER_MIND����������
					}
					break;
				}
				//ѧϰ�׶Σ������жϸ��᰸number�Ƿ���Learner��numberһ�£��������ͳ�ȥ
				case LEA_REQUEST:
				{
					if (msg._proposal._number > _learner._proposal._number) {
						vector<Message> to_send = _learner.process(msg);
						for (auto& m : to_send) {
							send_to_one(m, m._source);
						}
					}
					break;
				}
				//������Ӧ�ý��յ������Ϣ���ͣ����ھ���
				case NONE:
					break;
			}
		}
		//��Acceptor����һ��learn��Ϣ���һ����ѧϰ
		if(_learn_count == 1)
		{
			//����һ��Proposal����Ϣ�������ص�Acceptor
			Proposal learn_proposal(_acceptor._proposal);
			//��װ��Ϣ
			Message learn_request(LEA_REQUEST, learn_proposal, NUMBER_NULL, true, _id);
			//����Learner����������Ϣ
			vector<Message> to_send = _learner.process(learn_request);
			for (auto& msg : to_send) {
				send_to_one(msg, msg._source);
			}
		}
		//��Learner������µ�value
		{
			if (_learner._proposal._number > _number) {
				_value = _learner._proposal._value;
				_number = _learner._proposal._number;
				//����ҲҪ��ֵ_new_value��ֹ��
				//������ֵ���µ�ʱ���ٸ���_new_value�Ϳ�����
				_new_value = _value;
			}
		}
		//_learn_count����
		_learn_count = (_learn_count + 1) % LEARNING_TIME;
	}
public:
	vector<Server_Node>* server_list;		//ָ����Щ�̵߳�ָ��
	queue<VAL>		Value_Queue;			//���µ�Value
	queue<Message>	Message_Queue;			//��Ϣ����
	SID				_id;					//��ʶ
	Value			_value = VAL_NULL;		//ά����value
	Value			_new_value = VAL_NULL;	//�²�����value
	Number			_number = NUMBER_NULL;	//����value������number
	int				_learn_count = 0;		//ÿ������������ÿ��һ��Learning_time����һ��ѧϰ
private:
	Proposer	_proposer;
	Acceptor	_acceptor;
	Learner		_learner;
};