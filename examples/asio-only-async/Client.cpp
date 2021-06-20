#include "Client.h"
#include <regex>
#include <iostream>

const char* BasicGetRequest = "GET $(path) HTTP/1.1\nHost: $(host)\nAccept : */*\n\n";

/* This is a brute force way of doing this (but hey this is example code */
std::string ApplyTemplate(const std::string& template_file, const TemplateDefinitions& definitions)
{
	using namespace std::string_literals;
	std::string partial{ template_file };
	for (const auto replacement : definitions)
	{
		std::regex find_variable_regex("\\$\\("s + replacement.first + "\\)");   // matches just this variable
		const auto new_partial = std::regex_replace(partial, find_variable_regex, replacement.second); // replace variable->value
		partial = new_partial;
	}
	return partial;
}

Request Request::Make(const std::string& url)
{
	// Very basic URL matching. Not a full URL validator.
	std::regex re("https://([^/$:]+):?([^/$]*)(/?.*)");
	std::smatch what;
	if (!regex_match(url, what, re))
		return Request{};
	Request retval;
	retval.m_host = std::string(what[1]);
	retval.m_port = what[2].length() > 0 ? what[2].str() : "443";
	retval.m_path = what[3].length() > 0 ? what[3].str() : "/";
	return retval;
}



Client::Client()
	:m_stream(m_ioc, m_ctx)
{
	m_ctx.set_default_verify_paths();
	m_ctx.verify_server_certificate(true);
}

void Client::Connect(const Request& host)
{
	assert(m_state==State::Unconnected);
	m_state = State::Resolving;
	m_stream.set_server_hostname(winapi::MakeNative(host.m_host));
	m_resolver.async_resolve(host.m_host, host.m_port, 
		[this](const asio::error_code& err, const asio::ip::tcp::resolver::results_type& endpoints)
		{
			handle_resolve(err, endpoints);
		});
}

void Client::Get(const Request& req)
{
	assert(m_state == State::Connected);
	TemplateDefinitions defs{ {"host", req.m_host}, {"path", req.m_path} };
	m_last_request_text = ApplyTemplate(BasicGetRequest, defs);
	m_stream.async_write_some(asio::buffer(m_last_request_text.data(), m_last_request_text.size()), [&](asio::error_code ec, size_t size)
		{
			handle_send(ec, size);
		});
	m_state = Client::State::SendingGet;
}

void Client::Shutdown()
{
	assert(m_state == State::Connected);
	m_stream.async_shutdown([this](asio::error_code ec) {handle_shutdown_complete(ec); });
	m_state = State::Closing;
}

void Client::Update()
{
	m_ioc.run_one();
}


// state transition
void Client::handle_resolve(const asio::error_code& err,
	const asio::ip::tcp::resolver::results_type& endpoints)
{
	assert(m_state == State::Resolving);
	m_state = State::Resolving;
	if (!err)
	{
		// Attempt a connection to each endpoint in the list until we
		// successfully establish a connection.
		asio::async_connect(m_stream.next_layer(), endpoints,
			[this](asio::error_code err, const asio::ip::tcp::endpoint&) { handle_connect(err); });
		m_state = State::Connecting;
	}
	else
	{
		std::cout << "Error: " << err.message() << "\n";
		m_state = State::Error;
	}
}

void Client::handle_connect(asio::error_code err)
{
	assert(m_state == State::Connecting);
	if (!err)
	{
		m_stream.async_handshake(wintls::handshake_type::client, [this](asio::error_code ec) { handle_handshake_complete(ec); });
		m_state = State::Handshaking;
	}
	else
	{
		std::cout << "Error: " << err.message() << "\n";
		m_state = State::Error;
	}
}

void Client::handle_handshake_complete(asio::error_code err)
{
	assert(m_state == State::Handshaking);
	if (!err)
	{
		m_state = State::Connected;
	}
	else
	{
		std::cout << "Error: " << err.message() << "\n";
		m_state = State::Error;
	}
}

void Client::handle_send(asio::error_code err, size_t size)
{
	assert(m_state == State::SendingGet);
	if (!err)
	{
		m_last_request_text = m_last_request_text.substr(size);
		// if required send more
		if (!m_last_request_text.empty())
			m_stream.async_write_some(asio::buffer(m_last_request_text.data(), m_last_request_text.size()), [&](asio::error_code ec, size_t size)
				{
				handle_send(ec, size);
				});
		else
		{
			m_stream.async_read_some(asio::buffer(m_rx_buf.data(), m_rx_buf.size()), [&](asio::error_code ec, size_t size)
				{
					handle_rx(ec, size);
				});
			m_state = State::WaitingForReply;
		}
	}
	else
	{
		std::cout << "Error: " << err.message() << "\n";
		m_state = State::Error;
	}
}

void Client::handle_rx(asio::error_code err, size_t size)
{
	assert(m_state == State::WaitingForReply);
	if (!err)
	{
		// we should really wait for the end of the message, but assume it fits in one buffer (TODO: fix this)
		m_last_reply.insert(m_last_reply.end(), m_rx_buf.data(), m_rx_buf.data()+size);
		m_state = State::Connected;
	}
	else
	{
		std::cout << "Error: " << err.message() << "\n";
		m_state = State::Error;
	}
}


void Client::handle_shutdown_complete(asio::error_code err)
{
	assert(m_state == State::Closing);
	if (!err)
	{
		m_state = State::Unconnected;
	}
	else
	{
		std::cout << "Error: " << err.message() << "\n";
		m_state = State::Error;
	}
}

std::string to_string(Client::State state) 
{
	switch (state)
	{
#define HANDLE_STATE(X) case Client::State::X: return #X;
		HANDLE_STATE(Error)
		HANDLE_STATE(Unconnected)
		HANDLE_STATE(Resolving)
		HANDLE_STATE(Connecting)
		HANDLE_STATE(Handshaking)
		HANDLE_STATE(Connected)
		HANDLE_STATE(SendingGet)
		HANDLE_STATE(WaitingForReply)
		HANDLE_STATE(Closing)
#undef HANDLE_STATE
	}
	return "unknown Client::State";
}