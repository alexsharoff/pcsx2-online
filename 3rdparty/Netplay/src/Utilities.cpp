#include "Utilities.h"
#include <boost/asio.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/functional/hash.hpp>
#include <boost/thread.hpp>

void NETPLAY_API sleep( unsigned long long ms ) 
{
	boost::xtime xt; 
	boost::xtime_get(&xt, boost::TIME_UTC); 
	xt.nsec += ms*1000000;
	boost::thread::sleep(xt); 
}

unsigned long long NETPLAY_API getticks() {
	using namespace boost;
	using namespace boost::posix_time;

	time_duration diff = microsec_clock::universal_time() - 
		ptime(gregorian::date(2000,1,1));

	return diff.total_milliseconds();
}