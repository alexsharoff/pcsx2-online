#pragma once
#include <cstdint>



namespace shoryu
{
	//protocol id should be defined at session-level
	#define PROTOCOL_ID "SHR"
	const size_t PROTOCOL_ID_LEN = sizeof(PROTOCOL_ID)/sizeof(char);

	struct datagram_header
	{
		int64_t time;
		int64_t rtt;

		inline void set_defaults()
		{
			ack_first_id = 0;
			ack_mask = 0;
			rtt = 0;
			time = time_ms();
		}
		inline bool is_acknoledged(uint64_t id) const 
		{
			if(id < ack_first_id)
				return false;
			uint64_t offset = id - ack_first_id;
			if(offset >= sizeof(ack_mask)*8)
				return false;
			return (ack_mask & (1 << offset)) > 0;
		}
		inline void acknoledge(int64_t id)
		{
			if(id <= 0)
				throw std::exception("invalid id");
			if(!ack_first_id)
			{
				ack_first_id = id;
				ack_mask = 1;
			}
			else
			{
				int64_t offset = id - ack_first_id;
				if(offset >= 32)
					throw std::exception("offset cannot exceed 32");
				ack_mask |= 1 << offset;
			}
		}
		inline void serialize(oarchive& a) const
		{
			a.write(PROTOCOL_ID, PROTOCOL_ID_LEN);
			a << time << rtt << ack_first_id << ack_mask;
		}
		inline void deserialize(iarchive& a)
		{
			char pid[PROTOCOL_ID_LEN];
			a.read(pid, PROTOCOL_ID_LEN);
			if(std::memcmp(PROTOCOL_ID, pid, PROTOCOL_ID_LEN) == 0)
				a >> time >> rtt >> ack_first_id >> ack_mask;
			else
				throw std::exception("invalid protocol id");
		}
	protected:
		uint64_t ack_first_id;
		uint32_t ack_mask;
	};
}
