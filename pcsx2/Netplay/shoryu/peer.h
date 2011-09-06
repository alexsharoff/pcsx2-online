#pragma once

#include <list>
#include <boost/circular_buffer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include "datagram_header.h"

namespace shoryu
{
	
	template<typename MsgType>
	struct peer_data
	{
		peer_data()
			: rtt_avg(-1),rtt_max(-1),rtt_min(-1),remote_time(0),recv_time(0)
		{
		}
		endpoint ep;
		uint64_t remote_time;
		uint64_t recv_time;

		int32_t rtt_avg;
		int32_t rtt_max;
		int32_t rtt_min;
	};

	template<typename MsgType>
	class peer
	{
	private:  // emphasize the following members are private
      peer( const peer& );
      const peer& operator=( const peer& );
		struct msg_wrapper
		{
			int64_t id;
			MsgType data;
			inline void serialize(oarchive& a) const
			{
				a << id;
				data.serialize(a);
			}
			inline void deserialize(iarchive& a)
			{
				a >> id;
				data.deserialize(a);
			}
		};
		typedef std::list<msg_wrapper> container_type;
	public:
		peer() : received_msgs(32),next_id(1){}
		peer_data<MsgType> data;

		inline uint64_t queue_msg(const MsgType& msg)
		{
			boost::unique_lock<boost::mutex> lock(_mutex);
			msg_wrapper m;
			m.id = next_id;
			++next_id;
			m.data = msg;
			msg_queue.push_back(m);
			if(msg_queue.size() >= 10)
				msg_queue.pop_front();
			return m.id;
		}

		template<typename Pred>
		inline void deserialize_datagram(iarchive& ia, Pred& yield)
		{
			std::list<MsgType> data_list;
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				data.recv_time = time_ms();
				datagram_header header;
				header.deserialize(ia);
				data.remote_time = header.time;
				/*log_m.lock();
				log() << "ack from " << data.ep.port() << ":\t";*/
				msg_queue.remove_if([&](const msg_wrapper& msg) { return header.is_acknoledged(msg.id);});
				/*log() << std::endl;
				log_m.unlock();*/
				//FIX
				if(header.rtt != 0)
					estimate_rtt( (int32_t)(time_ms() - header.rtt));
				container_type::size_type msg_count;
				ia >> msg_count;

				/*log_m.lock();
				log() << "from " << data.ep.port() << ":\t";*/
				repeat(msg_count)
				{
					msg_wrapper msg;
					msg.deserialize(ia);
					if(std::find(received_msgs.begin(), received_msgs.end(), msg.id) == received_msgs.end())
					{
						//log() << "+" << msg.id << ", ";
						received_msgs.push_back(msg.id);
						data_list.push_back(msg.data);
					}
					else
					{
						//log() << "-" << msg.id << ", ";
					}
				}
				//log() << std::endl;
				//log_m.unlock();
			}
			foreach(MsgType& data, data_list)
				yield(data);
		}

		inline int serialize_datagram(oarchive& oa)
		{
			boost::unique_lock<boost::mutex> lock(_mutex);
			datagram_header header;
			header.set_defaults();
			/*
			log_m.lock();
			log() << "ack to " << data.ep.port() << ":\t";*/
			if(received_msgs.begin() != received_msgs.end())
			{
				std::sort(received_msgs.begin(), received_msgs.end());
				received_msgs_type::value_type min = *received_msgs.begin();
				for(auto it = received_msgs.begin(); it != received_msgs.end(); ++it)
				{
					if(*it - min> 31)
						break;
					header.acknoledge(*it);
					//log() << *it << ", ";
				}
			}
			/*log() << std::endl;
			log_m.unlock();*/

			if(data.remote_time != 0)
				header.rtt = data.remote_time + (time_ms() - data.recv_time);
			header.serialize(oa);
			oa << msg_queue.size();
			/*log_m.lock();
			log() << "to " << data.ep.port() << ":\t";*/
			foreach(msg_wrapper& msg, msg_queue) {
				//log() << msg.id << ", ";
				msg.serialize(oa);
			}
			/*log() << std::endl;
			log_m.unlock();*/
			return msg_queue.size();
		}
	private:
		container_type msg_queue;
		typedef boost::circular_buffer<uint64_t> received_msgs_type;
		received_msgs_type received_msgs;
		uint64_t next_id;
		inline void estimate_rtt(int32_t rtt)
		{
			if(data.rtt_min > rtt || data.rtt_min < 0)
				data.rtt_min = rtt;
			if(data.rtt_max < rtt || data.rtt_max < 0)
				data.rtt_max = rtt;

			if(data.rtt_avg < 0)
				data.rtt_avg = rtt;
			else
				data.rtt_avg = (3*rtt + 7*data.rtt_avg)/10;
		}
		boost::mutex _mutex;
	};
}