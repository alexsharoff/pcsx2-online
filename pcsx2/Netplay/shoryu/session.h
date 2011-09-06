#pragma once
#include <boost/thread/condition_variable.hpp>
#include "async_transport.h"

namespace shoryu
{
	enum MessageType : uint8_t
	{
		None,
		Frame,
		Ping, //for pinging
		Join,
		Deny,
		Info, //side, all endpoints, delay
		Wait,
		Delay, //set delay
		Ready //send to eps, after all eps answered - start game
	};

	template<typename T, typename StateType>
	struct message
	{
		typedef std::vector<endpoint> endpoint_container;
		typedef boost::asio::ip::address_v4 address_type;

		message() {}

		message(MessageType type) : cmd(type)
		{
		}
		MessageType cmd;
		StateType state;
		int64_t frame_id;
		std::vector<endpoint> eps;
		endpoint host_ep;
		uint32_t rand_seed;
		uint8_t delay;
		uint8_t side;
		uint8_t peers_needed;
		uint8_t peers_count;
		T frame;

		inline void serialize(shoryu::oarchive& a) const
		{
			a << cmd;
			switch(cmd)
			{
			case Join:
				a << state << host_ep.address().to_v4().to_ulong() << host_ep.port();
				break;
			case Deny:
				a << state;
				break;
			case Wait:
				a << peers_needed << peers_count;
				break;
			case Frame:
				a << frame_id;
				frame.serialize(a);
				break;
			case Info:
				a << rand_seed << side << eps.size();
				foreach(const endpoint& ep, eps)
					a << ep.address().to_v4().to_ulong() << ep.port();
				a << state;
				break;
			case Delay:
				a << delay;
			default:
				break;
			}
		}
		inline void deserialize(shoryu::iarchive& a)
		{
			a >> cmd;
			unsigned long addr;
			unsigned short port;
			switch(cmd)
			{
			case Join:
				a >> state >> addr >> port;
				host_ep = endpoint(address_type(addr), port);
				break;
			case Deny:
				a >> state;
				break;
			case Wait:
				a >> peers_needed >> peers_count;
				break;
			case Frame:
				a >> frame_id;
				frame.deserialize(a);
				break;
			case Info:
				endpoint_container::size_type size;
				a >> rand_seed >> side >> size;
				repeat(size)
				{
					a >> addr >> port;
					eps.push_back(endpoint(address_type(addr), port));
				}
				a >> state;
				break;
			case Delay:
				a >> delay;
			default:
				break;
			}
		}
	};

	template<typename FrameType, typename StateType>
	class session 
	{
		typedef message<FrameType, StateType> message_type;
		typedef std::vector<endpoint> endpoint_container;
		typedef boost::unordered_map<int64_t, FrameType> frame_map;
		typedef boost::unordered_map<endpoint, int> side_map;
		typedef std::vector<frame_map> frame_table;
		typedef std::function<bool(const StateType&, const StateType&)> state_check_handler_type;
	public:
#ifdef SHORYU_ENABLE_LOG
		std::stringstream log(ios_base::in + ios_base::out);
#endif
		bool bind(int port)
		{
			try
			{
				_async.start(port, 1);
			}
			catch(boost::system::system_error&)
			{
				return false;
			}
			return true;
		}
		void unbind()
		{
			_async.stop();
		}

		bool create(int players, const StateType& state, const state_check_handler_type& handler, int timeout = 0)
		{
			try_prepare();
			_state = state;
			_state_check_handler = handler;
			_async.receive_handler(boost::bind(&session<FrameType, StateType>::create_recv_handler, this, _1, _2));
			bool connected = true;
			if(create_handler(players, timeout) && _current_state != None)
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Established! ";
#endif
				connection_established();
			}
			else
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] NotEstablished! ";
#endif
				connected = false;
				_current_state = None;
				_async.receive_handler(boost::bind(&session<FrameType, StateType>::recv_hdl, this, _1, _2));
			}
			return connected;
		}
		bool join(endpoint ep, const StateType& state, const state_check_handler_type& handler, int timeout = 0)
		{
			try_prepare();
			_state = state;
			_state_check_handler = handler;
			_async.receive_handler(boost::bind(&session<FrameType, StateType>::join_recv_handler, this, _1, _2));
			bool connected = true;
			if(join_handler(ep, timeout) && _current_state != None)
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Established! ";
#endif
				connection_established();
			}
			else
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] NotEstablished! ";
#endif
				connected = false;
				_current_state = None;
				_async.receive_handler(boost::bind(&session<FrameType, StateType>::recv_hdl,this, _1, _2));
			}
			return connected;
		}
		void set(const FrameType& frame)
		{
			boost::unique_lock<boost::mutex> lock(_mutex);
			_frame_table[_side][_frame+_delay] = frame;
			message_type msg(Frame);
			msg.frame_id = _frame+_delay;
			msg.frame = frame;
			foreach(auto ep, _eps)
				_async.queue(ep, msg);
			
			send();
		}
		void send()
		{
			foreach(auto ep, _eps)
				_async.send(ep);
		}
		bool get(int side, FrameType& f, int timeout = 0)
		{
			if(_current_state == None)
				throw std::exception("invalid state");
			if(_frame < _delay)
				return true;
			boost::unique_lock<boost::mutex> lock(_mutex);
			if(timeout > 0)
			{
				if(!_frame_cond.timed_wait(lock, boost::posix_time::millisec(timeout),
					[&]() -> bool {
					if(_current_state != None)
						return _frame_table[side].find(_frame) != _frame_table[side].end();
					else
						return true;
				}))
				{
					return false;
				}
			}
			else
			{
				_frame_cond.wait(lock, [&]() -> bool {
					if(_current_state != None)
						return _frame_table[side].find(_frame) != _frame_table[side].end();
					else
						return true;
				});
			}
			if(_current_state == None)
				throw std::exception("invalid state");
			f = _frame_table[side][_frame];
			return true;
		}
		void delay(int d)
		{
			_delay = d;
		}
		int delay()
		{
			return _delay;
		}
		void next_frame()
		{
			_frame++;
		}
		int64_t frame()
		{
			return _frame;
		}
		void frame(int64_t f)
		{
			_frame = f;
		}
		int side()
		{
			return _side;
		}
		void shutdown()
		{
			clear();
			_frame_cond.notify_all();
			_connection_sem.post();
			//_connection_sem.clear();
		}
		int port()
		{
			return _async.port();
		}
		MessageType state()
		{
			return _current_state;
		}
		endpoint_container endpoints()
		{
			return _eps;
		}
		int64_t first_received_frame()
		{
			return _first_received_frame;
		}
		int64_t last_received_frame()
		{
			return _last_received_frame;
		}
	protected:
		void try_prepare()
		{
			clear();
		}
		void clear()
		{
			_last_received_frame = -1;
			_first_received_frame = -1;
			_delay = _side = /*_players =*/ 0;
			_frame = 0;
			_current_state = None;
			//_host = false;
			_eps.clear();
			_frame_table.clear();
			_async.error_handler(std::function<void(const error_code&)>());
			_async.receive_handler(std::function<void(const endpoint&, message_type&)>());
#ifdef SHORYU_ENABLE_LOG
			log.str("");
#endif
		}
		void connection_established()
		{
			_frame_table.resize(_eps.size() + 1);
			_async.error_handler(boost::bind(&session<FrameType, StateType>::err_hdl, this, _1));
			_async.receive_handler(boost::bind(&session<FrameType, StateType>::recv_hdl, this, _1, _2));
			sleep(delay() * 50 + 4 * 32);
		}
		int calculate_delay(uint32_t rtt)
		{
			return (rtt / 32) + 1;
		}

		struct peer_info
		{
			MessageType state;
			uint64_t time;
			int delay;
		};

		typedef std::map<endpoint, peer_info> state_map;

		state_map _states;
		MessageType _current_state;	
		unsigned int _players_needed;
		endpoint _host_ep;
		boost::semaphore _connection_sem;
		boost::mutex _connection_mutex;
		static const int connection_timeout = 1000;
		StateType _state;
		state_check_handler_type _state_check_handler;
		int64_t _first_received_frame;
		int64_t _last_received_frame;

		bool create_handler(int players, int timeout)
		{
			_players_needed = players;
			_current_state = Wait;
			if(timeout)
				return _connection_sem.timed_wait(timeout);
			else
				_connection_sem.wait();
			return true;
		}
		void create_recv_handler(const endpoint& ep, message_type& msg)
		{
			boost::unique_lock<boost::mutex> lock(_connection_mutex);

			if(msg.cmd == Join)
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Join ";
#endif
				if(!_state_check_handler(_state, msg.state))
				{
					message_type msg(Deny);
					msg.state = _state;
					_async.queue(ep, msg);
					_async.send(ep);
					return;
				}
				if(_current_state == Wait)
				{
					peer_info pi = { Join, time_ms(), 0 };
					_states[ep] = pi;
				}
				std::vector<endpoint> ready_list;
				ready_list.push_back(msg.host_ep);
				foreach(auto kv, _states)
				{
					if((time_ms() - kv.second.time < 1000) && kv.second.state == Join)
						ready_list.push_back(kv.first);
					if(ready_list.size() >= _players_needed )
						break;
				}
				if(ready_list.size() >= _players_needed)
				{
					if(_current_state == Wait)
					{
						message_type msg;
						msg.cmd = Info;
						msg.rand_seed = (uint32_t)time(0);
						msg.eps = ready_list;
						msg.state = _state;
						_eps = ready_list;
						for(size_t i = 0; i < _eps.size(); i++)
							_sides[_eps[i]] = i;

						_eps.erase(std::find(_eps.begin(), _eps.end(), _eps[_side]));
						srand(msg.rand_seed);
						for(size_t i = 1; i < ready_list.size(); i++)
						{
							msg.side = i;
							_async.queue(ready_list[i], msg);
						}
						_current_state = Ping;
						_side = 0;
					}
					for(size_t i = 1; i < ready_list.size(); i++)
						_async.send(ready_list[i]);
				}
			}
			if(msg.cmd == Ping)
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Ping ";
#endif
				message_type msg;
				msg.cmd = None;
				_async.queue(ep, msg);
				_async.send(ep);
			}
			if(msg.cmd == Delay)
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Delay ";
#endif
				peer_info pi = { Delay, 0, msg.delay };
				_states[ep] = pi;

				int ready = 0;
				int d = 0;
				foreach(auto kv, _states)
				{
					if(kv.second.state == Delay)
					{
						d += kv.second.delay;
						if( ++ready == (_players_needed - 1))
						{
							d /= _players_needed - 1;
							break;
						}
					}
				}
				if(ready == (_players_needed - 1))
				{
					_current_state = Ready;
					message_type msg(Delay);
					msg.delay = d;
					delay(d);
					for(size_t i = 0; i< _eps.size(); i++)
						_async.queue(_eps[i], msg);

					repeat(4)
					{
						bool ready = true;
						for(size_t i = 0; i< _eps.size(); i++)
						{
							if(_async.send(_eps[i]))
								ready = false;
						}
						if(ready)
							break;
						sleep(32);
					}
					_connection_sem.post();
				}
			}
		}

		bool join_handler(const endpoint& host_ep, int timeout)
		{
			_host_ep = host_ep;
			msec start_time = time_ms();
			do
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Connecting ";
#endif
				if(timeout > 0 && (time_ms() - start_time > timeout))
					return false;
				message_type msg(Join);
				msg.host_ep = host_ep;
				msg.state = _state;
				if(!_async.send(host_ep))
				{
					_async.queue(host_ep, msg);
					_async.send(host_ep);
				}
			}
			while(!_connection_sem.timed_wait(50));

			if(_current_state == Deny)
				return false;

			int i = 400;
			while(i-->0)
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Pinging ";
#endif
				foreach(endpoint& ep, _eps)
				{
					if(!_async.send(ep))
					{
						_async.queue(ep, message_type(Ping));
						_async.send(ep);
					}
				}
				shoryu::sleep(17);
			}

			int rtt = 0;
			foreach(auto ep, _eps)
			{
				auto peer = _async.peer(ep);
				if(rtt < peer.rtt_avg)
					rtt = peer.rtt_avg;
			}

			message_type msg(Delay);
			msg.delay = calculate_delay(rtt);
			_async.queue(host_ep, msg);

			int try_count = 10;
			while(_async.send(host_ep))
			{
#ifdef SHORYU_ENABLE_LOG
				log << "[" << time_ms() << "] Statecheck ";
#endif
				if(_current_state == Ready)
					return true;
				if(try_count-- == 0)
					return false;
				sleep(17);
			}
			return false;
		}
		void join_recv_handler(const endpoint& ep, message_type& msg)
		{
			if(ep != _host_ep)
				return;
			boost::unique_lock<boost::mutex> lock(_connection_mutex);
			if(msg.cmd == Info)
			{
				_side = msg.side;
				_eps = msg.eps;
				for(size_t i = 0; i < _eps.size(); i++)
					_sides[_eps[i]] = i;
				_eps.erase(std::find(_eps.begin(), _eps.end(), _eps[_side]));
				std::srand(msg.rand_seed);
				_current_state = Info;
				_state_check_handler(_state, msg.state);
				_connection_sem.post();
			}
			if(msg.cmd == Deny)
			{
				_current_state = Deny;
				_state_check_handler(_state, msg.state);
				_connection_sem.post();
			}
			if(msg.cmd == Delay)
			{
				if(_current_state != Ready)
				{
					delay(msg.delay);
					_current_state = Ready;
				}
				_async.queue(ep, message_type(Ready));
				_async.send(ep);
			}
			if(msg.cmd == Ping)
			{
				message_type msg;
				msg.cmd = None;
				_async.queue(ep, msg);
				_async.send(ep);
			}
		}
		
		void recv_hdl(const endpoint& ep, message_type& msg)
		{
			if(msg.cmd == Frame && _sides.find(ep) != _sides.end())
			{
				int side = _sides[ep];
				boost::unique_lock<boost::mutex> lock(_mutex);
				_frame_table[side][msg.frame_id] = msg.frame;
				if(_first_received_frame < 0)
					_first_received_frame = msg.frame_id;
				else if(msg.frame_id < _first_received_frame)
					_first_received_frame = msg.frame_id;

				if(_last_received_frame < 0)
					_last_received_frame = msg.frame_id;
				else if(msg.frame_id > _last_received_frame)
					_last_received_frame = msg.frame_id;
				_frame_cond.notify_all();
				/*log_m.lock();
				log() << "FRAME FROM " << ep.port() << ": " << msg.frame_id << std::endl;
				log_m.unlock();*/
			}
		}
		void err_hdl(const error_code& error)
		{
			std::string str = error.message();
		}
	private:
		int _delay;
		int64_t _frame;
		int _side;

		side_map _sides;
		async_transport<message_type> _async;
		endpoint_container _eps;
		frame_table _frame_table;
		boost::mutex _mutex;
		boost::condition_variable _frame_cond;
	};
}