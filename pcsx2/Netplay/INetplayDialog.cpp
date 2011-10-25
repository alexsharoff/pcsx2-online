#include "PrecompiledHeader.h"
#include <boost/thread/condition_variable.hpp>
#include <boost/shared_ptr.hpp>
#include "INetplayDialog.h"
#include "gui/NetplayDialog.h"
#include "App.h"

enum NetplayConfigurationPhase
{
	None,
	Settings,
	Confirmation,
	Ready
};

class ConcreteNetplayDialog: public INetplayDialog
{
public:
	void SetCloseEventHandler(const event_handler_type& handler)
	{
		_close_handler = handler;
	}
	event_handler_type& GetCancelEventHandler()
	{
		return _close_handler;
	}
	void SetSettings(const NetplaySettings& settings)
	{
		m_dialog->SetSettings(settings);
	}
	const NetplaySettings& GetSettings()
	{
		return m_dialog->GetSettings();
	}
	void Initialize()
	{
		if(m_dialog)
			m_dialog.reset();
		m_dialog.reset(new NetplayDialog((wxWindow*)GetMainFramePtr()));
		_phase = Settings;
		m_dialog->SetOKHandler([&] () {
			m_dialog->GetContent()->Disable();
			m_dialog->EnableOK(false);
			_operation_success = true;
			if(_phase == Settings)
			{
				_phase = Confirmation;
				if(_settings_ready_handler)
					_settings_ready_handler();
			}
			else
				_cond->notify_one();
		});
		m_dialog->SetCloseEventHandler([&] () { 
			try
			{
				_operation_success = false;
				_cond->notify_one();
				if(_close_handler)
					_close_handler();
			}
			catch(...){}
		});
	}
	bool Show()
	{
		_cond.reset(new boost::condition_variable());
		return m_dialog->Show();
	}
	bool IsShown()
	{
		return m_dialog;
	}
	bool Close()
	{
		if(m_dialog)
		{
			_operation_success = false;
			_cond->notify_one();
			m_dialog.reset();
			return true;
		}
		else
			return false;
	}
	void SetConnectionSettingsHandler(const event_handler_type& handler)
	{
		_settings_ready_handler = handler;
	}
	int WaitForConfirmation()
	{
		if(_phase != Confirmation)
			throw std::exception("invalid state");
		boost::unique_lock<boost::mutex> lock(_mutex);
		_cond->wait(lock);
		_phase = _operation_success ? Ready : None;
		if(_operation_success)
			return m_dialog->GetInputDelayPanel().GetInputDelay();
		else
			return -1;
	}
	void OnConnectionEstablished(int input_delay)
	{
		if(_phase != Confirmation)
			throw std::exception("invalid state");
		m_dialog->EnableOK(true);
		InputDelayPanel& p = m_dialog->GetInputDelayPanel();
		p.SetInputDelay(input_delay);
		p.Enable(GetSettings().FinetuneDelay && GetSettings().Mode == HostMode);
		m_dialog->SetContent(&p);
	}
	int GetInputDelay()
	{
		return m_dialog->GetInputDelayPanel().GetInputDelay();
	}
	void SetInputDelay(int input_delay)
	{
		m_dialog->GetInputDelayPanel().SetInputDelay(input_delay);
	}
	void SetStatus(const wxString& status)
	{
		m_dialog->SetStatus(status);
	}
protected:
	boost::shared_ptr<NetplayDialog> m_dialog;
	boost::shared_ptr<boost::condition_variable> _cond;
	boost::mutex _mutex;
	bool _operation_success;
	NetplayConfigurationPhase _phase;
	event_handler_type _close_handler;
	event_handler_type _settings_ready_handler;
};


INetplayDialog* INetplayDialog::GetInstance()
{
	if(!instance)
		instance = new ConcreteNetplayDialog();
	return instance;
}

INetplayDialog* INetplayDialog::instance = 0;