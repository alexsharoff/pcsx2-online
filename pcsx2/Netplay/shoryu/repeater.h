#pragma once
#include <boost/asio/deadline_timer.hpp>
#include <boost/bind.hpp>
#include <functional>

class repeater
{
public:
	typedef std::function<bool()> predicate_type;
	typedef std::function<void()> operation_type;
	repeater(const operation_type& op) : _op(op), _timer(_io_service), _timeout(1000)
	{
	}
	predicate_type& predicate()
	{
		return _pred;
	}
	void predicate(const predicate_type& pred)
	{
		_pred = pred;
	}
	void execute()
	{
		_op();
	}
	void run()
	{
		_io_service.reset();
		if(run_handler())
			_io_service.run();
	}
	int timeout()
	{
		return _timeout;
	}
	void timeout(int ms)
	{
		 _timeout = ms;
	}
private:
	bool run_handler()
	{
		if(!_pred || _pred())
		{
			_timer.expires_from_now(boost::posix_time::milliseconds(_timeout));
			execute();
			_timer.async_wait(boost::bind(&repeater::run_handler, this));
			return true;
		}
		return false;
	}
	boost::asio::io_service _io_service;
	boost::asio::deadline_timer _timer;
	predicate_type _pred;
	operation_type _op;
	int _timeout;
};