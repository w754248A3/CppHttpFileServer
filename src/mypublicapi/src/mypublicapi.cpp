#include "mypublicapi.h"
#define WIN32_LEAN_AND_MEAN   
#include <windows.h>
#include <shlwapi.h>
//#define WC_ERR_INVALID_CHARS 0x0080
//#define URL_ESCAPE_AS_UTF8              0x00040000
//#define URL_UNESCAPE_AS_UTF8            URL_ESCAPE_AS_UTF8




std::wstring MyWin32Out::GetWin32ErrorMessage(DWORD errorCode) {

	wchar_t buffer[4096];

	auto length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errorCode, 0, buffer, sizeof(buffer), nullptr);

	return std::wstring{ buffer, length };
}


std::wstring UTF8::GetWideCharFromUTF8(const std::string &s) {

	
	auto buffer = s.data();

	auto size = static_cast<int>(s.size());
	
	auto codePage = CP_UTF8;

	constexpr auto FLAG = MB_ERR_INVALID_CHARS;

	
	if (size < 0) {

		throw Win32SysteamException{ L"size overflow", 0 };
	}
	else if(size ==0){
		return std::wstring{};
	} 
	else
	{

		auto length = MultiByteToWideChar(static_cast<UINT>(codePage), FLAG, buffer, size, nullptr, 0);

		if (0 == length) {
			auto error = GetLastError();
			throw Win32SysteamException{L"GetWideCharFromUTF8 0 == length", error};
		}
		else {

			std::wstring ret_s{};

			ret_s.resize(static_cast<size_t>(length));

			if (length != MultiByteToWideChar(static_cast<UINT>(codePage), FLAG, buffer, size, ret_s.data(), length)) {

				auto error = GetLastError();
				throw Win32SysteamException{L"GetWideCharFromUTF8 length error", error};
			}
			else {

				return ret_s;
			}


		}

	}
}


std::string UTF8::GetUTF8FromWideChar(const std::wstring& s) {

	auto buffer = s.data();

	auto size = static_cast<int>(s.size());

	auto codePage = CP_UTF8;

	auto flag = WC_ERR_INVALID_CHARS;

	if (size < 0) {

		throw Win32SysteamException{ L"size overflow" ,0};
	}
	else if(size == 0){
		return "";
	}
	else {

		auto length = WideCharToMultiByte(static_cast<UINT>(codePage), static_cast<DWORD>(flag), buffer, size, nullptr, 0, nullptr, nullptr);

		if (0 == length) {
			auto error = GetLastError();
			throw Win32SysteamException{L"GetUTF8FromWideChar 0 == length", error};
		}
		else {
			
			std::string ret_s{};

			ret_s.resize(static_cast<size_t>(length));

			if (length != WideCharToMultiByte(static_cast<UINT>(codePage), static_cast<DWORD>(flag), buffer, size, ret_s.data(), length, nullptr, nullptr))
			{
				auto error = GetLastError();
				throw Win32SysteamException{L"GetUTF8FromWideChar length error", error};
			}
			else{
				return ret_s;
			}
			

		}
	}
}


