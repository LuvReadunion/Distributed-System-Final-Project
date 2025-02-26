# 分布式大作业报告

作者：**Luv**

## 前言

这是我本学期以来花费精力最大的一次作业了。

本次作业实现的是**分布式文件系统**。

从零开始手动实现一个**C/S模式**的分布式文件系统，客户端连接到服务器之后，可以通过**RPC**功能进行各种有关文件以及文件系统的一系列操作，在服务器发生单点故障可以通过**日志功能**进行维护，且将客户所能操作范围局限在一个**root**文件夹下以保证安全性，并对于文件、日志的访问设有**锁控制**。

最花费精力的尝试是实现基于**Paxos**算法实现的一致性算法，通过维护一个本地的线程集群来模拟多个文件服务器之间的达成一致的过程。为此，专门去阅读了**Leslie Lamport** 的原论文**《Paxos Made Simple》**，并写了一篇翻译注解随报告附上，希望能有助于之后的同学学习研讨**Paxos**这类一致性算法。

## 建立一个分布式文件系统

本次实验没有使用任何已有的**PRC**框架，而是只采用**TCP**套接字的方法进行客户端与服务端的通信。这一部分由于比较基础且实现内容比较杂，就简单概括一下。

> 全程代码均为手写，代码会在作业提交后在**github**上开源。
>
> 由于项目的测试都是在**Windows**上运行的，故使用的均为**Windows**系统库，如要移植到**Linux**上就需要改变部分系统库的调用。

### 文件的上传下载

由于是命令行操作，那么当客户发送下载文件的请求时就要先附上文件名。服务端根据文件名检查文件存在，然后返回能否提供文件的提示以及文件的尺寸。

所以需要先规定一个 **尽量简单的格式**：(类似协议)

#### 下载

1. 客户端设置目标**IP**地址以及端口号（省略）；
2. 客户端发送指令：`getfile + <完整文件名>`；
3. 服务端检索文件名，成功则发送应答：`find successfully, num: + <文件大小>`；
4. 服务端分批次传输文件。

#### 上传

1. 客户端设置目标**IP**地址以及端口号（省略）；
2. 客户端发送指令：`uploadfile + <完整文件名>`；
3. 服务端发送应答：`To uploading file.`；
4. 客户端隐式分析文件大小并发送；
5. 客户端分批次传输文件。

> 必须以二进制形式读写文件，使得非文本文件在传输之后不会损坏。

具体使用指令如下：

<img src="images.assets\{CDB910AC-B51F-4193-BF3D-11CCF2874835}.png" alt="{CDB910AC-B51F-4193-BF3D-11CCF2874835}" style="zoom:50%;" />

实现了文件的上传和下载，那么文件服务器就有了最基础的**文件备份**功能。

### root文件夹的限定

用户应该只能操作由服务器给定的范围内的资源(即**root**文件夹)。

也就是说，对于每个连接的用户，服务器应该**维护一个其所在位置的地址**，这个地址要包含在**root**文件夹内。

通过设置`cd`、`ls`这些指令使得客户能在**root**范围内调整其所在位置：

<img src="images.assets\{93066EC5-ED35-483E-90A8-08EA9C4A97CB}.png" alt="{93066EC5-ED35-483E-90A8-08EA9C4A97CB}" style="zoom:50%;" />

通过将用户限定在**root**范围内，保证了服务器的其他文件对用户**不可见**，用户也能通过建立、修改文件夹的方式使得其文件存放保持有序。

> 不过这样的设计还是因为后面关于**cmd**功能**接口**的**提供**而出现一些未知的漏洞。

### 基于cmd实现的功能与提供给客户的接口

要实现服务器端的文件访问功能，必然需要借助一些系统调用接口，最容易想到的就是**Windows**提供的**cmd**接口。

一个**C**程序可以调用**cmd**的系统库，通过管道功能传回**cmd**运行的结果。

通过这个思路，可以实现很多功能：

- 获取文件目录；
- 在服务端直接创建、修改、删除文件；
- 获取文件详细信息。

同时，我想到直接把**cmd**的系统调用进行封装，变成当前文件系统向用户的接口，用户通过`cmd + <cmd指令>`可以直接远程调用服务器端的cmd指令，这样就给予了用户极大的自定义空间。

> 当然，这个接口也是上文提到未知漏洞的由来。

### 文件树与文件列表

这一部分其实实现的比较晚，具体的专业描述也不太清楚(汗)，最终的实现选择是文件列表。

其实，最开始的设想是维护一颗文件树，但是最终还是没能实现出来，也算是一个遗憾吧。

> 若要维护一个动态的文件列表树，这棵树的结构为：
>
> - 根节点为**root**文件夹；
> - 每个内部节点为子文件夹；
> - 每个叶子节点为文件实体。
>
> 当服务器进程开始工作时，读取该树形，在之后每次对文件进行操作后对这棵树进行修正。
>
> 建立这样一棵树有几个目的：
>
> - 方便每个叶子节点在被访问时归该节点进行**上锁**；
> - 这样一颗文件树将会作为**Paxos**算法维护的**value**。
>
> 对于这样的结构单独建立一个`file_tree`类。

最终实现的是**文件列表**，直接将所有文件的访问路径存在一个列表中，修改某个文件的时候，对该文件上锁。(这个在后面的文件锁部分再提及)

### 日志

通过维护日志可以在服务器发生故障之后进行差错。

简单起见，使用唯一一个`log.txt`文件记录服务端的运行结果。

核心是实现**3**个函数：

- 更新日志文件`log_update()`；
- 输出日志文件`log_out()`；
- 清空日志文件`log_clear()`。

同时我们还要规定日志中每条**记录**的格式：`时间 + 指令 + 结果 + 客户端IP、端口`。

当然，客户端的连接与断开也应该记录在日志当中，替换以上格式里的“**指令**”。

需要通过连接的`ClientSocket`找到客户端的**IP**地址、端口号作为记录。

当日志记录出错时，一般客户端并不需要知道记录出错，因此更新部分不会再返回消息给客户端。日志是实时记录的，**写日志的时间即是事件发生的时间**。

多个线程如果同时写同一个日志文件，那就完蛋了。为了防止文件写出错的情况要上**锁**。(多线程后面实现)

定义一个全局锁`mx_log`，防止写文件出错。(这个锁是独立于后面提到的文件锁的。)

使用客户端接收日志既可以通过直接下载日志文件的方式，也可以调用额外设置的`log_show`指令观看。(适用于日志记录较少的情况)

由于采用客户端远程操控的方式控制日志的删除，所以真正文件的删除是做不到的(因为客户端的退出就会产生日志文件)，所以所谓的**删除**并不删除文件，只是将它的内容清空而已。使用`log_clear`指令。

> 对于日志的删除操作，最好设置一些权限。

### 多线程以连接多客户

这一步主要分为几步：

- 服务器为每个客户的接入设置一个单独的线程。
- 建立文件锁。

开辟多线程通过**线程池**就能得以实现。

这里以**日志**的形式展现多线程下写出的日志：

<img src="images.assets\{B5FD2D49-A0CA-4E71-B7AA-1CCFA1E01C6E}.png" alt="{B5FD2D49-A0CA-4E71-B7AA-1CCFA1E01C6E}" style="zoom:33%;" />

> 可以看到连接的**端口号**不同，即对应了不同的客户。

问题在于**文件锁**。要使得不同客户同时操作某个文件时不产生冲突，就必须要为该文件上锁，也就是说需要建立一个文件系统的“**锁表**”。

这样一个锁表是全局性的，它的涵盖范围应该在**root**文件夹内。

在服务端进程开始运行时，载入一个**文件表文件**，这个文件存储了根目录下所有文件的文件访问路径。

文件锁分为两种：共享锁(**读锁**)和排他锁(**写锁**)，其中只有共享锁是相容的。

> 这里简单起见，只建立**排他锁**。

为此，建立一个`file_list.h`文件专门编写文件列表相关的操作。

> 值得一提的是，对于`file_list.txt`文件本身，也建立一个专门的读写锁，因为这套体系有个非常重要的地方就是确保`file_list.txt`文件的正确性。

`file_list`是人为设置的一个由路径名映射到锁的**map**，其中实现为存储指向锁的指针。

关于`file_list`操作如下：

- 服务器初始化时，载入当前各个文件的记录，开辟节点相应的锁；
- 文件添加时，开辟节点和相应的锁；
- 文件删除时，释放锁所占空间并从**map**中移除。
- 在读、写文件前，根据**map**映射找到对应的锁并尝试上锁，该锁在行为结束后释放。

> 客户端在使用`cmd`指令接口时，不会使用本系统中定义的锁，而是由**cmd**系统调用的锁提供支持。

### 缓存的实现

缓存并不是我实现的重点，毕竟目前没有需要性能优化的需求。

如果想基于磁盘实现缓存的话，应该是要在跨主机上实现才有意义。

由于这里客户端对文件的修改方式是先下载后上传以覆盖原有文件的方式(当然也可以基于提供的**cmd**接口来对文件进行在线修改)，个人认为这样就是缓存机制的一种实现。区别在于下载文件不一定会上传，为了达到这一个要求也不难，新增一个”**修改**“指令，这个指令的执行逻辑是：先将文件下载下来，然后确认修改完成后进行提交上传覆盖。

最终的实现想法为`update_file` + `commit` + `push`三个指令，使用`update_file`抓取的文件在载入本地的**cache**文件夹后由客户端自行进行修改，然后可以对修改后的文件进行若干次`commit`操作，会在客户端的**to_commit**文件夹中生成当前版本的文件，最终使用`push`指令将**cache**中的文件上传并覆盖之。

> 实际实现还增加了很多简写，比如`update_file`可简写为`pull`。

大致效果如图：

<img src="images.assets\29c7cb6cbbe281db84b5ee82dd5217d.png" alt="29c7cb6cbbe281db84b5ee82dd5217d" style="zoom:33%;" />

> 缓存里内容在客户端退出的时候**自动删除**。

### 其余的实现

指令一多起来就不好记了，写一个本地的帮助文档，方便随时使用`help`指令查看：

<img src="images.assets\{3C4ED76E-9EF9-47A5-9365-3214D027599C}.png" alt="{3C4ED76E-9EF9-47A5-9365-3214D027599C}" style="zoom: 33%;" />

然后把**TCP**套接字连接的**IP**地址、端口号参数放在`ipconfig.txt`文件中，可以通过在该文件中修改来改变连接的地址。(程序在发起连接前会自动检测该文件，如有捕获参数则使用其替代默认参数。)

以上，使用大约**1400**行代码建立了一个多功能的分布式文件系统。

> 报告写得很**简短**，因为要把更多的篇幅留给**Paxos**，但是实现真的挺费工夫的。
>
> 实现到这里已经花费了很多时间，但是却是越做越起劲啊！

## Paxos一致性算法

真正的重头戏来了。

虽然有关于多个主机做服务器集群的情况最终是选择使用**多个线程**来模拟(确实这样实现有点滑稽)，但是尝试维护其中的一致性还是很有挑战性的。

先简要描述一下我对**Paxos**算法的个人理解：

- 首先这是一个非常牛的算法，共识问题是一个困难的问题，而**Leslie Lamport**却用了一个很简单的算法来解决，这是非常可贵的。对于该算法理解的成本很大，并不是源于这个算法本身的复杂度，而源于问题本身的复杂度。
- **Paxos**算法维护的**value**是一种**高度的抽象**。也就是说，在当前这个文件系统一致性的问题上，可以把之前提到的“**文件列表**”当作**value**，而且**value**的含义不局限于这个变量的“**值**”，而应该包括该变量的整个生命周期。
- 当**value**表征为一组值的时候，算法会演变为**Multi_Paxos**；而**Raft**算法又是基于**Multi_Paxos**算法特化出来的。**Paxos**和**Raft**都有**leader**的存在，但**Paxos**允许多个**leader**同时存在(限制较弱)，而**Raft**只维持有一个**leader**。

> 更多的理解和解读参见附上的**pdf**文件和亲手绘制的流程图。

那么该如何具体实现呢？

先来明确一些实现的要求：

- 每个服务器节点同时充当**Proposer**、**acceptor**、**learner**，于是决定对这三个角色分别建立类类型，然后都包含在一个**server_node**大类中；
- 关于序列号**number**，由于要保证**number**的产生有序而不重复，因此这里直接采用统一的时钟来充当**number**；
- 节点之间的通信采用**消息队列**的方式，给每个节点建立的一个单独的队列作为“**收信箱**”；
- 消息的内容使用一个结构体**message**实现，其中包含**proposal**、**ack**、**value**等变量来承载信息。**proposal**应该作为一个单独的实体出现，因为它将经过两个阶段。

### 传递消息的实现

首先定义一个`Paxos.h`文件，以下操作均在该文件中实现。

由于**Value**和**Number**都是抽象，所以这里采用抽象的类定义方式，再采用宏定义的方式给出这些抽象的具体实现。这样设计能使得需求和实现**解耦**。

#### Number

先从简单的开始做起，设置一个**number**，其值由构造的时间决定：

```C++
//写成宏定义形式保留抽象形式
#define NUM unsigned long long
#define NUM_NULL 0

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
	void operator = (const Number& num) {
		this->_number = num._number;
	}
public:
	NUM		_number;
};

//NUMBER的NULL常量
static const Number NUMBER_NULL = Number(NUM_NULL);
```

#### Value

**Value**原本是一个抽象，这里让其成为之前指向“**文件列表**”的指针：

```C++
//写成宏定义形式保留抽象形式
#define VAL map<string, mutex*>*
#define VAL_NULL nullptr

//Value类
struct Value {
public:
    Value() : _value(VAL_NULL){ }
	Value(VAL val) : _value(val) { }
public:
	VAL _value;
};
```

#### Proposal

然后设置提案**proposal**的格式，它应该由一个**number**和一个**value**组成：

```C++
//Proposal类
struct Proposal {
public:
	Proposal() : _number(NUMBER_NULL) { _value = VAL_NULL; }
	Proposal(VAL val) :_value(val) { }
public:
	Number	_number;
	Value	_value;
};
```

#### Message

对于消息类，它应该可以包含一个**proposal**、**ack**以及发送方**ID**信息：

```C++
//Message类
struct Message {
public:
	Message(){ }
	Message(Message_Type type, Proposal proposal, Number num = NUMBER_NULL, bool ack = false, SID s = SID_NULL)
		: _type(type), _proposal(proposal), _number(num), _ack(ack), _source(s) { }
public:
	Message_Type	_type;
	Proposal		_proposal;
	Number			_number;
	bool			_ack;
	SID				_source;
};
```

其中，**Message_Type**是指消息类型，纵观整个流程，可以设计以下的消息类型：

- **new_proposal**，只是方便编程的形式定义，表明这个消息是控制它的**Server_node**传递给它的，表示这是一个形式定义在**Message**里的**proposal**（这个**proposal**只会使用它的**value**）。
- **prepare request**，来自**Proposer**，包含一个处于**prepare**阶段的**proposal**；
- **accept request**，来自**Proposer**，包含一个处于**accrept**阶段的**proposal**；
- **prepare reply**，来自**Acceptor**，包含一个确认信息**ack**，并且可能包含**Acceptor**当前最大的**number**以及其已接受的最新的**proposal**；
- **accept reply**，来自**Acceptor**，包含一个确认信息**ack**；
- **learn request**，来自**Acceptor**或**Learner**，包含一个**Proposal**。

这里如此定义：

```C++
//消息类型枚举
enum Message_Type {
	NONE,
    NEVER_MIND,
    PRO_INVALID,
    NEW_PROPOSAL，
	PRE_REQUEST,
	ACC_REQUEST,
	PRE_REPLY,
	ACC_REPLY,
	LEA_REQUEST
};
```

> **NONE**只有在出错的时候才会出现，**NEVER_MIND**表示该消息可以忽略，**PRO_INVALID**表示提案已经无效了。
>
> 关于**Learn_Request**后面再决定如何让消息在**Learner**中传播。

### 心跳、发送与接收

要模拟各个服务节点之间收发消息。关于发送，是由**Leader**接收到客户端请求触发的，而关于接收消息，这里**不希望**每个服务节点处于**一直等待消息**的状态，于是引入的“**心跳**”机制，这个机制会在**Server_Node**里实现。

#### Acceptor

这里为了逻辑清晰先来实现**Acceptor**，它需要存储一个已接受的最新的**proposal**和目前同意的最高的**number**，并且需要对进入的消息进行处理，且产生一个回复的消息：

```C++
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
```

#### Proposer

接着是**Proposer**，构造类的思路应该与**Acceptor**类似。原理上说它只需要记得一个**Proposal**即可(因为这里维护的**Value**是唯一的，所以不会有同一个**number**下产生多个**proposal**的情况)。但实际上，还应该保留一个**number**变量来记录它意识到的最大的**number**，以便产生**number**更大的**proposal**。

当收到**ack == false**的响应时，**proposal**应该放弃这个提案，向上级发送**PRO_INVALID**的消息。

```C++
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
```

这里没有给**acc_count**设计实际的功能，因为**learner**的起点在**acceptor**，这里仅是保留一个接口用于**Proposal**发现消息超时丢失而重传。

> **Proposal**的工作写的我很头疼。

#### Learner

**Learner**需要规定一个**学习的传递规则**。

这里我们这样子规定，对于编号为**SID_MIN**到**SID_MAX**之间的**Server_Node**，每个**Learner**向从自身编号开始，依次向比自己编号大**1~LEARN_NUM**的**Learner**传递消息，对于每个版本的**proposal**只发送一次。

> 实际运行做了一个处理：将**Learn**的消息在**number**变量处设置**TTL**，每次发送递减，为**0**则不再往下传递，防止陷入循环。

当前的实例我们如此设置这些值：

```C++
#define SID int
#define SID_NULL 0
#define SID_MIN  1
#define SID_MAX  9
#define MAJORITY 5
#define LEARN_NUM 3
```

**Learner**这个本身只需要存储最新的**proposal**和**SID**即可。但是我们还需让**Acceptor**作为这一条学习路径的起点发送消息，这个需要后面通过**Server_Node**这个统一的身份进行两者的协调。

这里我们做一个变通，类型为**LEA_REQUEST**的消息的**SID**记录消息发送的对象。(原本记录的是消息的来源。)

```C++
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
```

#### Server_Node

接下来我们要把上面的三个角色统一起来。

> 这里需要先假设客户访问的是**SID**为**SID_MIN**的代理，也就是从这个节点开始发起**proposal**。

每个**Server_Node**应该有的功能如下：

- 在有新的**value**产生时，产生一个**Proposal**并放入该算法的**Proposer**起点；
- 每隔一段时间(**心跳**)查看消息队列，基于消息类型转发给本地的三种角色；
- 每隔一段时间(**心跳**)以**Acceptor**的身份产生一个**Learn**消息给**Learner**完成学习；
- 每隔一段时间(**心跳**)从**Learner**处获得最新的**value**。

简单起见，**心跳**的实现采用一个单独的线程轮流执行各个**Server_Node**的处理主函数。

**心跳**函数：

```C++
//产生心跳的单独线程
void heartbeat() {
	SID next_id = SID_MIN;
	while (true) {
		//产生心跳
		server_list[next_id - SID_MIN].process();
		//心跳间隔
		Sleep(100);
		//下一次产生心跳的SID
		next_id = (next_id + 1 - SID_MIN) % (SID_MAX - SID_MIN + 1) + SID_MIN;
	}
}
```

**Server_Node**类最终的实现如下：

```C++
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
			//printf("SID:%d 接收到消息:type = %d num = %d,SID = %d, ack = %d, val = %d\n",
			//	_id, msg._type, msg._number, msg._source, msg._ack, msg._proposal._value);
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
```

### 接入文件系统

来尝试接入文件系统吧！

> 关于实际的实现可能需要基于**file_list**进行文件的备份，但这里没有去实现，只是**拷贝一个列表**模拟一个多节点一致。

这里把心跳的频率调的较慢，这里只观察一个地方，就是：

**指向文件列表的指针**。

这个值作为**Value**，初始均为**null**，我们设置客户端在第**3**号服务节点接入，即此**Server_Node**变成了指向真实**file_list**的地址。

每一次心跳，打印出每个**Server_Node**的**Value**，观看变化。

<img src="images.assets\{E3661E43-3C7C-45F3-9EC6-29F4499E6FEC}.png" alt="{E3661E43-3C7C-45F3-9EC6-29F4499E6FEC}" style="zoom:50%;" />

可以看到，这里指针的值由默认空指针变成有值，且多个节点逐渐达到一致。

接着，我们尝试打印每个节点在心跳时接收到的所有**Message**：

|                             开始                             |                             中间                             |                             后期                             |
| :----------------------------------------------------------: | :----------------------------------------------------------: | :----------------------------------------------------------: |
| ![{F2FC5E19-062A-4965-A22E-971B7994BB8A}](images.assets\{F2FC5E19-062A-4965-A22E-971B7994BB8A}.png) | ![{FFBA83D4-C050-4900-ADE8-13EE9F6F936D}](images.assets\{FFBA83D4-C050-4900-ADE8-13EE9F6F936D}.png) | ![{4615420B-7898-4452-A98C-F81E4337E622}](images.assets\{4615420B-7898-4452-A98C-F81E4337E622}.png) |

> 这里为了清晰展示就不打印**Learner**的消息了。

可以看到，**SID == 3**的节点充当了**Proposer**的角色，因此它接收的消息是最特殊的。

出现消息类型对应的枚举如下：

|        4         |        5         |        6         |        7         |
| :--------------: | :--------------: | :--------------: | :--------------: |
| **准备阶段请求** | **接受阶段请求** | **准备阶段回复** | **接受阶段回复** |

以上就是对**Paxos**进行模拟的部分。

我把**Paxos**部分封装成了一个只有头文件的库，如果要使用只需要改动**Number**和**Value**的部分即可。

## 实验总结

整个实验做下来成果是非常丰富的。

前面的分布式文件系统部分总过程花了差不多一周时间，后面的**Paxos**算法从阅读原文开始到实现为头文件也花了将近一周的时间。

鉴于我还有另一门工程制图课程的作业是“挑选本专业一篇**Top**论文画一个全局图示”，这里干脆选择《**Paxos Made Simple**》全篇没有一张图的论文来绘制好了，绘制成品随报告附上。

关于该文件系统的使用，我导出了可执行文件，包含在**project**中，先后运行**Server**和**Client**，即可对里面的文件进行操作。通过修改`ipconfig.txt`文件并打开一个隧道，是可以实现远程连接的。

本学期的分布式系统课程上的其实很痛苦，但也确实学到了不少东西，在此感谢老师和助教了。

🥰🥰🥰🥰🥰
