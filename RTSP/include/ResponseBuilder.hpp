#pragma once
#include <string>
#include <string_view>

class ResponseBuilder
{
public:
	ResponseBuilder(int code, std::string_view reason, int cseq) 
	{
		data_.reserve(512);

		data_ += "RTSP/1.0 ";
		data_ += std::to_string(code);
		data_ += ' ';
		data_ += reason;
		data_ += "\r\n";

		header("CSeq", std::to_string(cseq));
	}

	ResponseBuilder& header(std::string_view key, std::string_view value) 
	{
		data_ += key;
		data_ += ": ";
		data_ += value;
		data_ += "\r\n";
		return *this;
	}

	std::string build(std::string_view body = {}) 
	{
		if (not body.empty()) 
		{
			header("Content-Length", std::to_string(body.size()));
			data_ += "\r\n";
			data_ += body;
		} else {
			data_ += "\r\n";
		}
		return std::move(data_);
	}

private:
	std::string data_;
};