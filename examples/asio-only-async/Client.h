#pragma once
#include <WinSock2.h>
#include <wintls.hpp>
#include <map>

using TemplateDefinitions = std::map<std::string, std::string>;
std::string ApplyTemplate(const std::string& template_file, const TemplateDefinitions& definitions);

struct Request
{
	static Request Make(const std::string&);
	std::string m_path;
	std::string m_port;
	std::string m_host;
};

class Client
{
public:
	Client();

	void Connect(const Request& host);
	void Get(const Request& host);
	void Shutdown();
	void Update();

	enum class State{ Error, Unconnected, Resolving, Connecting, Handshaking, Connected, SendingGet, WaitingForReply, Closing};
	State GetState() { return m_state; }
	const std::string& GetLastReply() { return m_last_reply; }
private:
	using socket = asio::ip::tcp::socket;
	asio::io_context					m_ioc;
	wintls::context						m_ctx{ wintls::method::system_default };
	asio::ip::tcp::resolver				m_resolver{ m_ioc };
	wintls::stream<socket>				m_stream;
	State								m_state{ State::Unconnected };
	std::string							m_last_request_text;
	std::array<char, 1024*10>			m_rx_buf;
	std::string							m_last_reply;
	// state changes
	void handle_resolve(const asio::error_code& err, const asio::ip::tcp::resolver::results_type& endpoints);
	void handle_connect(asio::error_code err);
	void handle_handshake_complete(asio::error_code err);
	void handle_send(asio::error_code err, size_t size);
	void handle_rx(asio::error_code err, size_t size);
	void handle_shutdown_complete(asio::error_code err);
};

std::string to_string(Client::State);