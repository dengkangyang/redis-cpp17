#include "log.h"
#include "tcpclient.h"
#include "tcpconnection.h"
const size_t kMaxHashSize = 10 * 1000 * 1000;

class MapWoker
{
public:
	MapWoker(EventLoop *loop,const char *ip1,
			 int port1,const char *ip2,int port2)
	:loop(loop),
	 count(2)
	{
		TcpClientPtr client1(new TcpClient(loop,this));
		client1->setConnectionCallback(std::bind(&MapWoker::connCallBack,this,
				std::placeholders::_1,std::placeholders::_2));
		client1->setMessageCallback( std::bind(&MapWoker::readCallBack,this,
				std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
		client1->connect(ip1,port1);
		tcpVecs.push_back(client1);

		TcpClientPtr client2(new TcpClient(loop,this));
		client2->setConnectionCallback(std::bind(&xMapWoker::connCallBack,this,
					std::placeholders::_1,std::placeholders::_2));
		client2->setMessageCallback( std::bind(&xMapWoker::readCallBack,this,
					std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
		client2->setConnectionErrorCallBack(std::bind(&xMapWoker::connErrorCallBack,this));
		client2->connect(ip2,port2);
		tcpVecs.push_back(client2);

		{
			std::unique_lock <std::mutex> lck(mutex);
			while(count > 0 )
			{
				condition.wait(lck);
			}
		}
		LOG_INFO<<"all connect success";
	}

	void connErrorCallBack()
	{

	}

	void readCallBack(const TcpConnectionPtr& conn,Buffer *recvBuf)
	{

	}

	void connCallBack(const TcpConnectionPtr &conn)
	{
		if(conn->connected())
		{
			std::unique_lock <std::mutex> lck(mutex);
			--count;
			condition.notify_one();
		}
		else
		{
			std::unique_lock <std::mutex> lck(mutex);
			if(++count == 2)
			{
				loop->quit();
			}
		}
	}

	bool  isconnectAll()
	{

	}

	void processFile(const char *fileName)
	{
		std::ifstream in(fileName);
		std::string word;

		while(in)
		{
			mapWorks.clear();
			while(in >> word)
			{
				mapWorks[word] += 1;
				if(mapWorks.size() > kMaxHashSize)
				{
					break;
				}
			}

			LOG_INFO<<"map work " <<mapWorks.size()<<" recude work";
			for(auto &it : mapWorks)
			{
				size_t index =  std::hash<std::string>()(it->first) % tcpVecs.size();
				buffer.append(it->first);
				char buf[64];
				snprintf(buf,sizeof buf,"\t%" PRId64 "\r\n",it->second);
				buffer.append(buf);
				tcpVecs[index]->connection->send(&buffer);
			}
		}

		for(auto &it : tcpVecs)
		{
			it->connection->shutdown();//close eagin  close write  ask  TCP fin
		}

		loop->quit();
	}
private:
	std::unordered_map<std::string,int64_t> mapWorks;
	EventLoop *loop;
	std::vector<TcpClientPtr> tcpVecs;
	std::condition_variable condition;
	std::mutex mutex;
	int count;
	Buffer buffer;
};

int main(int argc,char* argv[])
{
	if (argc < 5)
	{
		printf("Example: ip1 port1 ip2 port2 input_file1....... \n");
	}
	else
	{
		EventLoop loop;
		MapWoker work(&loop,argv[1],atoi(argv[2]),argv[3],atoi(argv[4]));
		work.processFile(argv[5]);
		loop.run();
	}

	return 0;
}
