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

//消息类型枚举
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

//Number类
struct Number {
public:
	Number() {		//number的值即为构造时的时间
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

//NUMBER的NULL常量
static const Number NUMBER_NULL = Number(NUM_NULL);

//Value类
struct Value {
public:
	Value() : _value(VAL_NULL){ }
	Value(VAL val) : _value(val) { }
public:
	VAL		_value;
};

//Proposal类
struct Proposal {
public:
	Proposal() : _number(NUMBER_NULL) { _value = VAL_NULL; }
	Proposal(VAL val) :_value(val) { }
	Proposal(Value val) :_value(val) { }
public:
	Number	_number;
	Value	_value;
};

//Message类
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
	//构造函数
	Proposer(SID id) : _id(id), _number(NUMBER_NULL) { }
	//消息的处理
	Message process(Message& msg) {
		//先判断消息类型
		switch (msg._type) {
			case NEW_PROPOSAL:	//新的提案，需要发送一阶段请求
				//读取该提案需求(value)
				_proposal = msg._proposal;
				//产生一个大于_number的number放入proposal中
				{
					Number new_num = Number();
					if (new_num < _number) {	//按照number的产生规律不应该出现矛盾
						printf("程序出错！出现不符合number规律的情况。\n");
						exit(0);
					}
					else {
						_proposal._number = new_num;
						//封装消息
						Message request(PRE_REQUEST, _proposal, NUMBER_NULL, true, _id);
						//期望回复的统计量置零
						pre_count = 0;
						//置为有效
						is_valid = true;
						return request;			//返回消息
					}
				}
			case PRE_REPLY:		//一阶段请求的回应
				//先判断该消息是否 ack == true
				if (msg._ack) {
					//判断是否有效，无效则跳过
					if (!is_valid) {
						//更新已知的最大number
						_number = msg._number;
						//封装消息
						Message request(PRO_INVALID, _proposal, _number, true, _id);
						return request;			//返回消息
					}
					//进行计数，并判断是不是达到majority的响应
					++pre_count;
					//如果达到了就可以发送下一阶段的请求
					if (pre_count >= MAJORITY) {
						//封装消息
						Message request(ACC_REQUEST, _proposal, NUMBER_NULL, true, _id);
						//期望回复的统计量置零(确保只返回一次这个消息)
						pre_count = 0;
						//接受阶段的统计量置零
						acc_count = 0;
						return request;			//返回消息
					}
					else {		//还未达到足够数量的响应
						//封装消息
						Message request(NEVER_MIND, _proposal, NUMBER_NULL, true, _id);
						return request;			//返回消息
					}
				}
				else {		//否则观察消息内容，得知最新的proposal和number
					//放弃当前提案
					is_valid = false;
					//更新已知的最大number
					_number = msg._number;
					//封装消息
					Message request(PRO_INVALID, _proposal, _number, true, _id);
					return request;			//返回消息
				}
			case ACC_REPLY:		//二阶段请求的回应
				//先判断提案是否有效，无效则向上通报
				if (!is_valid) {
					//更新已知的最大number
					_number = msg._number;
					//封装消息
					Message request(PRO_INVALID, _proposal, _number, true, _id);
					return request;			//返回消息
				}
				//先判断该消息是否 ack == true
				if (msg._ack) {
					//计数
					++acc_count;
					//封装消息
					Message request(NEVER_MIND, _proposal, NUMBER_NULL, true, _id);
					return request;			//返回消息
				}
				else {	//说明acceptor接收到更大number的proposal
					//放弃当前提案
					is_valid = false;
					//更新已知的最大number
					_number = msg._number;
					//封装消息
					Message request(PRO_INVALID, _proposal, _number, true, _id);
					return request;			//返回消息
				}
		}
		//未检测到相应类型则返回NONE消息
		return Message(NONE, _proposal, _number, false, _id);
	}
public:
	//如果需要实现Multi_Paxos，就需要把这里的proposal扩展成一个向量
	Proposal	_proposal;		//准备提交的proposal
	Number		_number;		//意识到的最大的number
	int			pre_count = 0;	//已确认准备的数量
	int			acc_count = 0;	//已确认接收的数量
	bool		is_valid;		//提案是否被放弃
	SID			_id;			//标识
};

class Acceptor {
public:
	//构造函数
	Acceptor(SID id) : _id(id), _number(NUMBER_NULL) { }
	
	//消息的处理
	Message process(Message & msg) {
		//先判断消息类型
		switch (msg._type) {
			case PRE_REQUEST:	//一阶段请求
				//判断这个proposal的number是不是大于当前的number
				if (msg._proposal._number > _number) {
					//更新当前的_number
					_number = msg._proposal._number;
					//构造回复的Message
					Message reply(PRE_REPLY, _proposal, NUMBER_NULL, true, _id);
					return reply;		//返回消息
				}
				else {	//否则返回ack = false
					//构造回复的Message
					Message reply(PRE_REPLY, _proposal, _number, false, _id);
					return reply;		//返回消息
				}
			case ACC_REQUEST:	//二阶段请求
				//判断这个proposal的number是不是等于当前的number
				if (msg._proposal._number == _number) {
					//接受该proposal
					_proposal = msg._proposal;
					//更新当前的_number(这步用来做一个优化，也是原本论文中的一个歧义点)
					_number = msg._proposal._number;
					//构造回复的Message
					Message reply(ACC_REPLY, _proposal, _number, true, _id);
					return reply;		//返回消息
				}
				else {	//否则返回ack = false
					//构造回复的Message
					Message reply(ACC_REPLY, _proposal, _number, false, _id);
					return reply;		//返回消息
				}
		}
		//未检测到相应类型则返回NONE消息
		return Message(NONE, _proposal, _number, false, _id);
	}

public:
	Proposal	_proposal;	//已接受的最新的proposal
	Number		_number;	//已回复的最大的number
	SID			_id;		//标识
};

class Learner {
public:
	Learner(SID id) : _id(id) { }
	//消息的学习
	vector<Message> process(Message& msg) {
		//这个消息只应该是Learn的消息
		if (msg._type == LEA_REQUEST) {
			//比较number,如果接收的number更大则取之否则忽略
			vector<Message> ret;
			if (msg._proposal._number > _proposal._number) {
				//更新proposal
				_proposal = msg._proposal;
				//构造学习的Message
				//向比自己编号大1~LEARN_NUM的Learner发送消息(注意这里_source的部分改为存放目的sid)
				for (int i = 1; i <= LEARN_NUM; ++i) {
					SID aim_id = (_id + i - SID_MIN) % (SID_MAX - SID_MIN + 1) + SID_MIN;
					ret.emplace_back(LEA_REQUEST, _proposal, msg._number, false, aim_id);
				}
			}
			return ret;			//返回消息
		}
		//未检测到相应类型则返回NONE消息
		return vector<Message>(1, Message(NONE, _proposal, NUMBER_NULL, false, _id));
	}
public:
	Proposal	_proposal;	//已接受的最新的proposal
	SID			_id;		//标识
};

class Server_Node {
public:
	Server_Node(SID id, vector<Server_Node>* ser_list)
		: _id(id), _proposer(id), _acceptor(id), _learner(id), server_list(ser_list) { }
	//将消息发给其他全部节点
	void send_to_all(const Message& msg) {
		for (int i = SID_MIN; i <= SID_MAX; ++i) {
			if (i != _id) {
				(*server_list)[i - SID_MIN].Message_Queue.push(msg);
			}
		}
	}
	//将消息发给其他某个节点
	void send_to_one(const Message& msg, SID aim_id) {
		(*server_list)[aim_id - SID_MIN].Message_Queue.push(msg);
	}
	//处理主函数
	void process() {
		//观察是否有新的value产生
		if (_new_value._value != _value._value) {
			//产生一个Proposal的消息发给本地的Proposer
			Proposal new_proposal(_new_value);
			//封装消息
			Message request(NEW_PROPOSAL, new_proposal, NUMBER_NULL, true, _id);
			//交给Proposer处理得到消息
			request = _proposer.process(request);
			//将消息发给其他全部节点
			send_to_all(request);
		}
		//处理消息队列中的消息
		while (!Message_Queue.empty()) {
			Message msg = Message_Queue.front();
			Message_Queue.pop();
			//debug演示，打印SID以及接收到的包信息
			if(msg._type != 8)
				printf("SID:%d 接收到消息:type = %d num = %d,SID = %d, ack = %d, val = %d\n",
					_id, msg._type, msg._number, msg._source, msg._ack, msg._proposal._value);
			switch (msg._type) {
				//可忽略的消息
				case NEVER_MIND:
					break;
				//当前提案失效
				case PRO_INVALID:
					//这里简单起见就不做处理了(最好是在一段时间后重发)
					break;
				//新的提案，正常不应该收到这个类型
				case NEW_PROPOSAL:
					break;
				//准备阶段请求，下放给Acceptor，回应消息转发给源节点
				case PRE_REQUEST:
				{
					Message reply = _acceptor.process(msg);
					send_to_one(reply, msg._source);
					break;
				}
				//接收阶段请求，下放给Acceptor，回应消息转发给源节点
				case ACC_REQUEST:
				{
					Message reply = _acceptor.process(msg);
					send_to_one(reply, msg._source);
					break;
				}
				//准备阶段的回应，下放给Proposer
				case PRE_REPLY:
				{
					Message reply = _proposer.process(msg);
					//如果得到的消息为ACC_REQUEST，就可以接着群发2阶段的Proposal消息
					if (reply._type == ACC_REQUEST) {
						send_to_all(reply);
					}
					//发现提案已失效
					else if (reply._type == PRO_INVALID) {
						;//简单起见不做处理
					}
					else {
						;//一般为NEVER_MIND，不做处理
					}
					break;
				}
				//接受阶段的回应，下放给Proposer
				case ACC_REPLY:
				{
					Message reply = _proposer.process(msg);
					//发现提案已失效
					if (reply._type == PRO_INVALID) {
						;//简单起见不做处理
					}
					else {
						;//一般为NEVER_MIND，不做处理
					}
					break;
				}
				//学习阶段，有则判断该提案number是否与Learner的number一致，更大则发送出去
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
				//正常不应该接收到这个消息类型，用于纠错
				case NONE:
					break;
			}
		}
		//让Acceptor产生一个learn消息完成一致性学习
		if(_learn_count == 1)
		{
			//产生一个Proposal的消息发给本地的Acceptor
			Proposal learn_proposal(_acceptor._proposal);
			//封装消息
			Message learn_request(LEA_REQUEST, learn_proposal, NUMBER_NULL, true, _id);
			//交给Learner产生发送消息
			vector<Message> to_send = _learner.process(learn_request);
			for (auto& msg : to_send) {
				send_to_one(msg, msg._source);
			}
		}
		//从Learner获得最新的value
		{
			if (_learner._proposal._number > _number) {
				_value = _learner._proposal._value;
				_number = _learner._proposal._number;
				//这里也要赋值_new_value防止震荡
				//当有新值更新的时候再更改_new_value就可以了
				_new_value = _value;
			}
		}
		//_learn_count自增
		_learn_count = (_learn_count + 1) % LEARNING_TIME;
	}
public:
	vector<Server_Node>* server_list;		//指向这些线程的指针
	queue<VAL>		Value_Queue;			//更新的Value
	queue<Message>	Message_Queue;			//消息队列
	SID				_id;					//标识
	Value			_value = VAL_NULL;		//维护的value
	Value			_new_value = VAL_NULL;	//新产生的value
	Number			_number = NUMBER_NULL;	//最新value附带的number
	int				_learn_count = 0;		//每次心跳递增，每逢一个Learning_time发起一次学习
private:
	Proposer	_proposer;
	Acceptor	_acceptor;
	Learner		_learner;
};