#include <WinSock2.h>
#include "Client.h"
#include <iostream>

void PollUntil(Client& HTTPSClient, Client::State& prev_state, Client::State target_state)
{
	while (prev_state != target_state)
	{
		const auto new_state = HTTPSClient.GetState();
		if (prev_state != new_state)
			std::cout << "HTTPSClient:" << to_string(HTTPSClient.GetState()) << std::endl;
		prev_state = new_state;
		if (prev_state == Client::State::Error)
			break;
		HTTPSClient.Update();
	}
}

int main(int argc, char** argv)
{
	// Exactly one command line argument required - the HTTPS URL
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " [HTTPS_URL]\n\n";
		std::cerr << "Example: " << argv[0] << " https://www.boost.org/LICENSE_1_0.txt\n";
		return EXIT_FAILURE;
	}

	// turn the commandline into a HTTP (Get) request
	Request new_request = Request::Make(argv[1]);
	if (new_request.m_host.empty())
	{
		std::cerr << "Usage: " << argv[0] << " [HTTPS_URL]\n\n";
		std::cerr << "Example: " << argv[0] << " https://www.boost.org/LICENSE_1_0.txt\n";
		return EXIT_FAILURE;
	}

	// make an objject to do the "getting"
	Client HTTPSClient;
	HTTPSClient.Connect(new_request);
	Client::State prev_state{ Client::State::Error };
	PollUntil(HTTPSClient, prev_state, Client::State::Connected);

	HTTPSClient.Get(new_request);
	prev_state = Client::State::Error;
	PollUntil(HTTPSClient, prev_state, Client::State::Connected);

	HTTPSClient.Shutdown();
	PollUntil(HTTPSClient, prev_state, Client::State::Unconnected);

	std::cout << "Reply:" << std::endl;
	std::cout << HTTPSClient.GetLastReply() << std::endl;
	return 0;
}