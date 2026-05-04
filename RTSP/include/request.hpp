#pragma once
#include <string_view>

struct request 
{
	std::string_view method;
	std::string_view uri;
	std::string_view version;

	int cseq = -1;

	std::string_view transport;
	std::string_view session;
	std::string_view user_agent;
};