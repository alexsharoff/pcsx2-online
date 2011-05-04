#include "Netplay.h"
#include <boost/thread.hpp>
#include "Utilities.h"

boost::mutex _m;

//встроенные в dll тесты
/*
void packetTest()
{
	using namespace std;
	PingPacket* ping = new PingPacket();
	stringstream ss(string(), ios_base::in | ios_base::out | ios_base::binary);
	assert( ping->toStream(ss));
	cout << "tellg=" << ss.tellg() << ", tellp=" << ss.tellp() << endl;
	delete ping;
	ping = new PingPacket();
	assert(ping->fromStream(ss));

	Frame* frame = new Frame(1, "test", 4);
	assert( memcmp(frame->getData(), "test", 4) == 0);
	assert( frame->id() == 1);
	ss.clear();
	assert(frame->toStream(ss));
	cout << "tellg=" << ss.tellg() << ", tellp=" << ss.tellp() << endl;
	delete frame;
	frame = new Frame();
	assert(frame->fromStream(ss));
	assert( memcmp(frame->getData(), "test", 4) == 0);
	assert( frame->id() == 1);

	InputPacket* input = new InputPacket();
	input->appendFrame(new Frame(0, "frame1", 6));
	input->appendFrame(new Frame(1, "FRAME_2", 7));
	assert(input->getFrameCount() == 2);
	assert( memcmp(input->getFrame(0)->getData(), "frame1", 6) == 0);
	assert( memcmp(input->getFrame(1)->getData(), "FRAME_2", 7) == 0);
	assert( input->getFrame(0)->id() == 0 );
	assert( input->getFrame(1)->id() == 1 );
	ss.clear();
	assert(input->toStream(ss));
	cout << "tellg=" << ss.tellg() << ", tellp=" << ss.tellp() << endl;
	delete input;
	input = new InputPacket();
	assert(input->fromStream(ss));
	assert(input->getFrameCount() == 2);
	assert( memcmp(input->getFrame(0)->getData(), "frame1", 6) == 0);
	assert( memcmp(input->getFrame(1)->getData(), "FRAME_2", 7) == 0);
	assert( input->getFrame(0)->id() == 0 );
	assert( input->getFrame(1)->id() == 1 );
}
*/

Endpoint EpClient, EpHost;

void hostThread()
{
	Netplay np;
	np.start(EpHost.port);
	if(np.host(5000))
	{
	}
	else
	{
		return;
	}
	using namespace std;

	/*cout << "minRtt=" << np.getPeers()[0]->stats.minRtt() << "; " << endl;
	cout << "maxRtt=" << np.getPeers()[0]->stats.maxRtt() << "; "
		<< "rtt=" << np.getPeers()[0]->stats.rtt() << "; "

		<< "recvCount=" << np.getPeers()[0]->stats.recvCount() << "; "
		<< "recvReached=" << np.getPeers()[0]->stats.recvReached() << "; "
		<< "sendCount=" << np.getPeers()[0]->stats.sendCount() << "; "
		<< "sendReached=" << np.getPeers()[0]->stats.sendReached() << "; "
		<< "recvOverlapped=" << np.getPeers()[0]->stats.recvOverlapped() << "; " << endl;*/

	int itc = 0;
	unsigned long long start = getticks();
	while(true)
	{
		stringstream ss;
		ss << itc;
		np.setInput(new Frame(itc,  ss.str().c_str(),ss.str().length()), itc, 0);
		np.sendInput(EpClient, 0);
		using namespace std;
		Frame* f = np.getInput(itc, 1, 5000);
		/*if(!f) {
			cout << "client timeout" << endl;
			break;
		}
		else
		{
			_m.lock();
			cout << "client: ";
			cout.write(f->getData(), f->dataLength());
			cout << endl;
			_m.unlock();
		}*/
		itc++;
		if(itc > 600)
			break;
		//sleep(17);
	}
	cout << "client end. Total time=" << (getticks() - start) << endl;

	char confirm[3];
	confirm[2] = 0;
	while(true)
	{
		np.send(EpClient, "OK", 2);
		int r = np.recv(EpClient, confirm, 2, 300);
		if(r > 0 && strcmp(confirm, "OK") == 0)
		{
			cout << "host OK" << endl;
			break;
		}
	}
	np.endSession();
	
}

void clentThread()
{
	Netplay np;
	np.start(EpClient.port);
	if(np.connect(EpHost.ip.c_str(), EpHost.port, 5000))
	{
	}
	else
	{
		return;
	}
	using namespace std;
	/*cout << "minRtt=" << np.getPeers()[0]->stats.minRtt() << "; "
		<< "maxRtt=" << np.getPeers()[0]->stats.maxRtt() << "; "
		<< "rtt=" << np.getPeers()[0]->stats.rtt() << "; "

		<< "recvCount=" << np.getPeers()[0]->stats.recvCount() << "; "
		<< "recvReached=" << np.getPeers()[0]->stats.recvReached() << "; "
		<< "sendCount=" << np.getPeers()[0]->stats.sendCount() << "; "
		<< "sendReached=" << np.getPeers()[0]->stats.sendReached() << "; "
		<< "recvOverlapped=" << np.getPeers()[0]->stats.recvOverlapped() << "; " << endl;*/
	
	int its = 0;
	unsigned long long start = getticks();
	while(true)
	{
		stringstream ss;
		ss << its;
		np.setInput(new Frame(its, ss.str().c_str(),ss.str().length()), its, 1);
		np.sendInput(EpHost, 1);
		using namespace std;
		Frame* f = np.getInput(its, 0, 5000);
		/*if(!f) {
			cout << "host timeout" << endl;
			break;
		}
		else
		{
			_m.lock();
			cout << "host: ";
			cout.write(f->getData(), f->dataLength());
			cout << endl;
			_m.unlock();
		}*/
		its++;
		if(its > 600)
			break;
		//sleep(17);
	}
	cout << "host end. Total time=" << (getticks() - start) << endl;

	char confirm[3];
	confirm[2] = 0;
	while(true)
	{
		np.send(EpHost, "OK", 2);
		int r = np.recv(EpHost, confirm, 2, 300);
		if(r > 0 && strcmp(confirm, "OK") == 0)
		{
			cout << "client OK" << endl;
			break;
		}
	}
	np.endSession();
}

int main(int argc, char* argv[])
{
	using namespace std;

	//packetTest();

	EpClient.ip = "127.0.0.1";
	EpClient.port = 3434;
	EpHost.ip = "127.0.0.1";
	EpHost.port = 4343;

	boost::thread t1(clentThread);
	boost::thread t2(hostThread);

	t1.join();
	t2.join();

	cin.get();
	return 0;
}
