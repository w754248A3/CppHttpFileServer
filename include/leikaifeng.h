#pragma once
#include <errhandlingapi.h>
#include <minwindef.h>
#include <utility>
#ifndef _LEIKAIFENG
#define _LEIKAIFENG

#include <iostream>
#include <string>
#include <functional>
#define WIN32_LEAN_AND_MEAN   
#include <windows.h>
#include <shlwapi.h>
//#define WC_ERR_INVALID_CHARS 0x0080
//#define URL_ESCAPE_AS_UTF8              0x00040000
//#define URL_UNESCAPE_AS_UTF8            URL_ESCAPE_AS_UTF8



template<typename TIn, typename TOut>
TOut Integer_cast(TIn v){
	
	if(std::cmp_greater(v, std::numeric_limits<TOut>::max()) 
	|| std::cmp_less(v, std::numeric_limits<TOut>::min())){
			throw std::overflow_error("Integer_cast Overflow");
	}
	else{
		return static_cast<TOut>(v);
	}
	
	
}


template<typename T>
concept NotStringOrChar =
!std::same_as<T, const std::string&> &&
!std::same_as<T, std::string> &&
!std::same_as<T, const char*>;

class UTF8{

public:


	static std::wstring GetWideCharFromUTF8(const std::string& s);

	static std::string GetUTF8FromWideChar(const std::wstring& s);

};


class MyWin32Out{

public:

	static void Print() {
		std::wcout << std::endl;
	}

	template<NotStringOrChar T, typename ...TS>
	static void Print(T value, TS ...values) {
		std::wcout << value << "   ";
		Print(values...);
	}

	static std::wstring GetWin32ErrorMessage(DWORD errorCode) {

		wchar_t buffer[4096];

		auto length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errorCode, 0, buffer, sizeof(buffer), nullptr);

		return std::wstring{ buffer, length };
	}

	static void Exit(const std::wstring& message, int errorCode) {
		Print(message, L"----", GetWin32ErrorMessage((DWORD)errorCode));
		
		exit(errorCode);
	}

	static void Exit(const std::wstring& message) {
		Print(message);
		exit(-1);
	}




};


class Win32SysteamException  {

	std::wstring m_message;
public:

	Win32SysteamException(const std::wstring& message, DWORD errorCode) :m_message() {

		m_message = message +L"__:__" +  MyWin32Out::GetWin32ErrorMessage(errorCode);
		
	}

	const wchar_t* what() const noexcept {
		return m_message.c_str();
	}
};

class Delete_Base {
public:
	
	Delete_Base() {}

	Delete_Base(const Delete_Base&) = delete;
	
	Delete_Base(Delete_Base&&) = delete;

	Delete_Base& operator=(const Delete_Base&) = delete;
	
	Delete_Base& operator=(Delete_Base&&) = delete;
};


#endif // !_LEIKAIFENG
