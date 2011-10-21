#pragma once
#include <boost/shared_ptr.hpp>
#include "NetplaySettings.h"


class INetplayDialog
{
protected:
	static INetplayDialog* instance;
public:
	static INetplayDialog& GetInstance();
	static void Create();

	virtual boost::shared_ptr<NetplaySettings> WaitForConnectionSettings() = 0;
	virtual int WaitForConfirmation() = 0;

	virtual void OnConnectionEstablished(int input_delay) = 0;
	virtual void SetStatus(const std::string& status) = 0;
private:
	virtual void Show() = 0;
	virtual void Close() = 0;
};
