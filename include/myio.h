#pragma once
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <minwindef.h>
#include <string_view>
#include <system_error>
#include <winnt.h>
#ifndef _MYIO
#define _MYIO

#include <limits>
#include <utility>
#include <iostream>
#include <algorithm>
#include <string>
#include <array>
#include <vector>
#include <memory>
#include <queue>
#include <unordered_map>
#include <thread>


#include <winsock2.h>
#define WIN32_LEAN_AND_MEAN   
#include <windows.h>
#include <ws2tcpip.h>
#include <mswsock.h>
//#include <WinDNS.h>
#include <wininet.h>
#include <mstcpip.h>
#include "leikaifeng.h"

#define MYTEXT(args) args

namespace mt {


using mystring = std::string;

using mystring_view= std::string_view;

using mychar = char;
}






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


void WSAExit(const std::string& message) {
	Exit(message, WSAGetLastError());
}


class MyFunc{

public:

	template<typename TRead, typename TWrite>
	requires (
		std::is_invocable_r_v<uint32_t, TRead, size_t, char**, uint32_t>&&
		std::is_invocable_r_v<uint32_t, TWrite, char*, uint32_t>)
	static void CopyTo(
		size_t readOffset,
		size_t needCopyCount,
		uint32_t const oneSendCount,
		TRead&& readfunc, 
		TWrite&& writefunc){

		
		while (needCopyCount > 0)
		{
			uint32_t count=0;
			if(needCopyCount >= oneSendCount){
				count=oneSendCount;


			}
			else{
				count = (uint32_t)needCopyCount;
			}

			char* buf=nullptr;
			uint32_t redCount = readfunc(readOffset, &buf, count);

			if(redCount ==0 || buf==nullptr){
				Print("file loop read 0");
				return;
			}

			uint32_t n = writefunc(buf, redCount);

			if(n == 0){
				return;
			}
			needCopyCount-=n;
			readOffset+=n;
		}
		
	}

	static void CopyTo_(
		std::function<uint32_t(char*, uint32_t, size_t)> readfunc, 
		std::function<uint32_t(char*, uint32_t)> writefunc,
		size_t offset, 
		DWORD count){

		const size_t SIZEBUFF = 2097152;
		//const size_t SIZEBUFF = 8192;
		auto buf = std::make_unique<char[]>(SIZEBUFF);
		
		while (count > 0)
		{
			
			auto redCount = readfunc(buf.get(), SIZEBUFF, offset);

			if(redCount ==0){
				Print("file loop read 0");
				return;
			}
			DWORD canSendCount =0;
			if(count > redCount){
				canSendCount=redCount;
			}
			else{
				canSendCount =count;
			}

			auto n = writefunc(buf.get(), canSendCount);


			count-=n;

			offset+=n;
		}
		
	}



};

class Win32SocketException : public Win32SysteamException {
public:
	using Win32SysteamException::Win32SysteamException;

	Win32SocketException(const std::string& message) : Win32SysteamException(message, (DWORD)WSAGetLastError()) {

	}
};

class SystemException : public std::exception {
std::string m_message;
public:

	SystemException(std::string message) : m_message(message) {

	}

	
	const char* what() const noexcept override {
		return m_message.c_str();
	}
};


class ArgumentException : public ::SystemException {


public:

	ArgumentException(std::string message) : SystemException(message) {

	}
};


template<typename ...TS>
class PBack {

};


template<typename T, typename ...TS>
class PBack<T, TS...> {
	T m_value;
	PBack<TS...> m_back;
public:

	PBack(T value, TS ...values)
		: m_value(value),
		m_back(values...) {

	}

	auto& GetValue() {
		return m_value;
	}

	auto& GetPBack() {
		return m_back;
	}


};

template <typename TF, typename T, typename ...TS, typename ...TPS>
constexpr void _Used_PBack_Call(TF tf, PBack<T, TS...> value, TPS ...tps) {
	if constexpr (sizeof...(TS) == 0) {
		tf(tps..., value.GetValue());
	}
	else {
		_Used_PBack_Call(tf, value.GetPBack(), tps..., value.GetValue());
	}
}

template <typename TF, typename ...TS>
constexpr void Used_PBack_Call(TF tf, PBack<TS...> value) requires(std::is_invocable_v<TF, TS...>) {
	if constexpr (sizeof...(TS) == 0) {
		tf();
	}
	else {
		
		if constexpr (sizeof...(TS) == 1) {
			tf(value.GetValue());
		}
		else {
			_Used_PBack_Call(tf, value.GetPBack(), value.GetValue());
		}
	}
}


enum class IOPortFlag : ULONG_PTR {
	
	FiberSwitch,
	
	FiberCreate,
	
	FiberDelete,

	FiberActionAdd
};

class Fiber;

class Info {

public:
	static auto CreateIPv4TcpSocket() {
		auto handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (handle == INVALID_SOCKET) {
			WSAExit("create socket error");

			throw Win32SocketException{};
		}
		else {
			return handle;
		}
	}

private:
	template<typename TF>
	static auto GetFunctionAddress(GUID guid) {


		auto handle = Info::CreateIPv4TcpSocket();

		TF functionAddress = nullptr;

		DWORD outSize = 0;

		auto result = ::WSAIoctl(
			handle, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid, sizeof(guid),
			&functionAddress, sizeof(functionAddress),
			&outSize, nullptr, nullptr);

		closesocket(handle);


		if (result == SOCKET_ERROR) {
			WSAExit("get function address error");

			throw Win32SocketException{};
		}
		else {
			return functionAddress;
		}
	}

	

	inline static LPFN_ACCEPTEX s_acceptex;

	inline static LPFN_CONNECTEX s_connectex;

	inline static LPFN_TRANSMITPACKETS s_transmitpackets;

	static void InitializationWSA() {
		WSADATA data;

		auto value = WSAStartup(MAKEWORD(2, 2), &data);

		if (value != 0) {
			Exit("Initialization error", value);
		}

		s_acceptex = Info::GetFunctionAddress<LPFN_ACCEPTEX>(WSAID_ACCEPTEX);

		s_connectex = Info::GetFunctionAddress<LPFN_CONNECTEX>(WSAID_CONNECTEX);

		s_transmitpackets = Info::GetFunctionAddress<LPFN_TRANSMITPACKETS>(WSAID_TRANSMITPACKETS);
	}

	
public:

	static auto& GetContentTypeMap() {

		static std::unordered_map<std::wstring_view, mt::mystring_view> map{};

		return map;
	}

	static void InitializationMap() {

		decltype(auto) map = GetContentTypeMap();


		map.emplace(L".html", MYTEXT("text/html"));
		map.emplace(L".htm", MYTEXT("text/html"));
		map.emplace(L".css", MYTEXT("text/css"));
		map.emplace(L".js", MYTEXT("application/javascript"));
		map.emplace(L".json", MYTEXT("application/json"));
		map.emplace(L".xml", MYTEXT("application/xml"));
		map.emplace(L".txt", MYTEXT("text/plain"));

		map.emplace(L".jpg", MYTEXT("image/jpeg"));
		map.emplace(L".jpeg", MYTEXT("image/jpeg"));
		map.emplace(L".png", MYTEXT("image/png"));
		map.emplace(L".gif", MYTEXT("image/gif"));
		map.emplace(L".bmp", MYTEXT("image/bmp"));
		map.emplace(L".svg", MYTEXT("image/svg+xml"));
		map.emplace(L".ico", MYTEXT("image/vnd.microsoft.icon"));
		map.emplace(L".webp", MYTEXT("image/webp"));

		map.emplace(L".mp3", MYTEXT("audio/mpeg"));
		map.emplace(L".wav", MYTEXT("audio/wav"));
		map.emplace(L".ogg", MYTEXT("audio/ogg"));
		map.emplace(L".m4a", MYTEXT("audio/mp4"));
		map.emplace(L".flac", MYTEXT("audio/flac"));

		map.emplace(L".mp4", MYTEXT("video/mp4"));
		map.emplace(L".mkv", MYTEXT("video/x-matroska"));
		map.emplace(L".webm", MYTEXT("video/webm"));
		map.emplace(L".avi", MYTEXT("video/x-msvideo"));
		map.emplace(L".mov", MYTEXT("video/quicktime"));
		map.emplace(L".flv", MYTEXT("video/x-flv"));
		map.emplace(L".ts", MYTEXT("video/vnd.iptvforum.ttsmpeg2"));

		map.emplace(L".pdf", MYTEXT("application/pdf"));
		map.emplace(L".zip", MYTEXT("application/zip"));
		map.emplace(L".rar", MYTEXT("application/vnd.rar"));
		map.emplace(L".7z", MYTEXT("application/x-7z-compressed"));
		map.emplace(L".tar", MYTEXT("application/x-tar"));
		map.emplace(L".gz", MYTEXT("application/gzip"));
		map.emplace(L".doc", MYTEXT("application/msword"));
		map.emplace(L".docx", MYTEXT("application/vnd.openxmlformats-officedocument.wordprocessingml.document"));
		map.emplace(L".ppt", MYTEXT("application/vnd.ms-powerpoint"));
		map.emplace(L".pptx", MYTEXT("application/vnd.openxmlformats-officedocument.presentationml.presentation"));
		map.emplace(L".xls", MYTEXT("application/vnd.ms-excel"));
		map.emplace(L".xlsx", MYTEXT("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"));

	}



	static void Initialization() {

		Info::InitializationWSA();

		Info::InitializationMap();

		auto& v = IsCallInitialization();

		v = true;

	}

	static bool& IsCallInitialization(){
		static bool v;

		return v;
	}

	static auto GetAcceptEx() {
		return s_acceptex;
	}

	static auto GetConnectEx() {
		return s_connectex;
	}

	static auto GetTransmitPackets() {
		return s_transmitpackets;
	}
};



class IPEndPoint {


	sockaddr_in m_value;

public:
	IPEndPoint(const char* ipstr, USHORT port) {
		sockaddr_in value = {};

		value.sin_family = AF_INET;

		value.sin_addr.s_addr = inet_addr(ipstr);
		
		value.sin_port = htons(port);

		m_value = value;
	}


	auto Get() const {
		return m_value;
	}
};


class OverLappedEx : public OVERLAPPED {
public:
	LPVOID other;
};



class Fiber : Delete_Base {
	
private:
	class IData;
	inline thread_local static Fiber* s_value;

	
public:
	template<typename ...TS>
	using FiberFuncType = std::decay_t<void(TS...)>;

	HANDLE m_io_over_port;

	std::deque<std::unique_ptr<IData>> m_data_queue;

	std::deque<LPVOID> m_fiber_queue;

	LPVOID m_main_fiber;

	Fiber():m_io_over_port{}, m_data_queue{}, m_fiber_queue{},  m_main_fiber{}{

		m_io_over_port = Fiber::CreateIoCompletionPort();
	}

	static Fiber& GetThis(){

		if(s_value == nullptr){
			Exit("fiber * is null");
		}

		return *s_value;
	}

private:

	class IData {
	public:
		virtual void Call() = 0;
		virtual ~IData() {}
	};

	template<typename ...TS>
	class Data : public IData {
	
		FiberFuncType<TS...> m_func;
		PBack<TS...> m_value;

	public:
		Data(FiberFuncType<TS...> func, TS ...value) : m_func(func), m_value(value...) {

		}

		void Call() override {
			
			Used_PBack_Call(m_func, m_value);
		}
	};


	
	static HANDLE CreateIoCompletionPort() {
		auto handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

		if (handle == nullptr) {
			Exit("create Io Completion Port error");

			throw Win32SysteamException{};
		}
		else {
			return handle;
		}
	}



	auto& GetPQueue() {
		return m_data_queue;
	}

	auto& GetFiberQueue() {
		return m_fiber_queue;
	}

	auto GetPortHandle(){
		return m_io_over_port;
	}

	constexpr static size_t FIBER_COUNT = 8;


	void Fiber_Func() {
		
		decltype(auto) pqueue = this->GetPQueue();


		auto p = std::move(pqueue.front());

		pqueue.pop_front();

		try {
			p->Call();
		}
		catch (Win32SocketException& e) {
			Print("fiber throw socket throw");

			Exit(e.what());
		}
		catch (Win32SysteamException& e) {
			Print("fiber throw system throw");
			Exit(e.what());
		}
		catch (std::exception& e) {
			Print("fiber throw exception throw");
			Exit(e.what());
		}
		catch (...) {
			Exit("fiber throw error");
		}
		
	}

	
	static void WINAPI Fiber_Func(LPVOID) {

		//本意是让参数在Fiber::ForwardFunc<T>方法中析构,因为当前方法不会正常结束
		//但不清楚编译器的实现


		while (true)
		{
			
			Fiber::GetThis().Fiber_Func();


			decltype(auto) queue =Fiber::GetThis().GetFiberQueue();
			
			if (queue.size() > Fiber::FIBER_COUNT)
			{

				 Fiber::GetThis().PostToIoCompletionPort(IOPortFlag::FiberDelete, GetCurrentFiber());

			}
			else {
				queue.push_back(GetCurrentFiber());
			}

			Fiber::GetThis().SwitchMain();
		}
		
	}

public:


	
	
	void AddToIoCompletionPort(HANDLE fileHandle) {
		auto handle = ::CreateIoCompletionPort(fileHandle, m_io_over_port, static_cast<ULONG_PTR>(IOPortFlag::FiberSwitch), 0);
		if (handle == nullptr) {
			Exit("add Io Completion Port error");
		}
	}

	void PostToIoCompletionPort(IOPortFlag flag, LPVOID value) {
		if (0 == ::PostQueuedCompletionStatus(m_io_over_port, 0, static_cast<ULONG_PTR>(flag), static_cast<LPOVERLAPPED>(value))) {
			Exit("post io Completion Port error");
		}
	}

	

	void Convert() {
		auto handle = ::ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);

		if (handle == nullptr) {
			Exit("Convert To Fiber Error");
		}
		else {
			m_main_fiber = handle;
		}
	}

	template <typename ...TS>
	void Create_ThreadSafe(FiberFuncType<TS...> func, TS ...value) {
		
		std::unique_ptr<IData> p = std::make_unique<Data<TS...>>(func, value...);

		auto pp = new std::unique_ptr<IData>{std::move(p)};



		Fiber::PostToIoCompletionPort(IOPortFlag::FiberActionAdd, pp);
	}

	template <typename ...TS>
	void Create(FiberFuncType<TS...> func, TS ...value) {
		
		//这个地方如果参数是万能引用会导致包装参数的类型字段也是引用

		std::unique_ptr<IData> p = std::make_unique<Data<TS...>>(func, value...);
		this->Create2(std::move(p));

	}
	void Create2(std::unique_ptr<IData> p)
	{
		Fiber::GetPQueue().push_back(std::move(p));

		decltype(auto) fiberqueue = Fiber::GetFiberQueue();

		LPVOID handle;

		if (fiberqueue.size() != 0) {

			handle = fiberqueue.front();

			fiberqueue.pop_front();
		}
		else {
			handle = ::CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, Fiber::Fiber_Func, nullptr);

			if (handle == nullptr) {
				Exit("Create Fiber Error");
			}
		}
		
		Fiber::PostToIoCompletionPort(IOPortFlag::FiberCreate, handle);
	}

	void PostMain(LPVOID fiber){

		Fiber::PostToIoCompletionPort(IOPortFlag::FiberCreate, fiber);
	}

	void SwitchMain() {
		Fiber::Switch(m_main_fiber);
	}

	void Switch(LPVOID fiber) {
		if (fiber == GetCurrentFiber()) {
			Exit("Switch Fiber error");
		}

		::SwitchToFiber(fiber);
	}

	void Delete(LPVOID fiber) {
		if (fiber == m_main_fiber) {
			Exit("delete fiber error");

		}

		::DeleteFiber(fiber);
	}

	template <typename... TS>
	void Start(Fiber::FiberFuncType<TS...> func, TS... value)
	{
		
		if(Info::IsCallInitialization() == false){
			Exit("can not call Initialization");
		}

		Fiber::s_value= this;

		Fiber::Convert();

		if(Fiber::s_value != this){
			Exit("convert fiber thread local data not eq");
		}

		Fiber::Create(func, value...);

		std::array<OVERLAPPED_ENTRY, 32> buffer{};

		DWORD count;
		auto id = ::GetCurrentThreadId();
		while (true)
		{
			
		
			auto res = GetQueuedCompletionStatusEx(Fiber::GetPortHandle(), buffer.data(), static_cast<ULONG>(buffer.size()), &count, INFINITE, true);
			
			if (TRUE !=res)
			{
				Exit("get io error");
			}
			else
			{
				for (DWORD i = 0; i < count; i++)
				{
					auto &item = buffer[i];

					auto flag = static_cast<IOPortFlag>(item.lpCompletionKey);

					if (flag == IOPortFlag::FiberSwitch)
					{

						Fiber::Switch(static_cast<OverLappedEx *>(item.lpOverlapped)->other);
					}
					else if (flag == IOPortFlag::FiberCreate)
					{

						Fiber::Switch(item.lpOverlapped);
					}
					else if (flag == IOPortFlag::FiberDelete)
					{
						Fiber::Delete(item.lpOverlapped);
					}
					else if (flag == IOPortFlag::FiberActionAdd)
					{
						
						auto p = reinterpret_cast<std::unique_ptr<IData>*>(item.lpOverlapped);

						this->Create2(std::move(*p));

						delete p;
					}
					else{
						Exit("can not define Fiber flag");
					}
				}
			}
		}
	}
};



class CreateReadOnlyFile : Delete_Base {

	HANDLE m_handle;

public:
	CreateReadOnlyFile(const std::wstring& path) {
		m_handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

		if (m_handle == INVALID_HANDLE_VALUE) {
			throw Win32SysteamException{};
		}
		Fiber::GetThis().AddToIoCompletionPort(reinterpret_cast<HANDLE>(m_handle));
	}

	auto GetSize() {
		LARGE_INTEGER size;
		if (GetFileSizeEx(m_handle, &size)) {
			return size.QuadPart;
		}
		else {
			throw Win32SysteamException{};
		}
	}

	auto Read(char* buf, DWORD size, size_t offsetCount){

		OverLappedEx overlapped = {};

		{
			LARGE_INTEGER offset = {};
			offset.QuadPart=Integer_cast<size_t, LONGLONG>(offsetCount);

			overlapped.Offset = offset.LowPart;
			overlapped.OffsetHigh =static_cast<DWORD>(offset.HighPart);
		}
		
		
		overlapped.other = GetCurrentFiber();
		
		auto ret = ::ReadFile(m_handle, buf, size, nullptr, &overlapped);
		auto e = GetLastError();
		if(ret != 0 || e != ERROR_IO_PENDING){
			throw Win32SysteamException{"read file error:", e};
		}

		Fiber::GetThis().SwitchMain();
	
		DWORD count;

		{

			auto ret = GetOverlappedResult(m_handle, &overlapped, &count, false);

			auto e = GetLastError();

			if (ret) {
				
				return static_cast<ULONG>(count);
			}
			else {

				throw Win32SocketException{ "Read file over error:", e };
			}
		}

		

	}

	auto GetHandle() {
		return m_handle;
	}

	~CreateReadOnlyFile()
	{
		CloseHandle(m_handle);
		
	}
};



class TcpSocket : Delete_Base {
	SOCKET m_handle;
	bool is_close;
	ULONG Read(char* buffer, ULONG size, DWORD flag) {
		this->OnClose_Throw();

		WSABUF buf = {};

		buf.buf = buffer;

		buf.len = size;

		OverLappedEx overlapped = {};

		overlapped.other = GetCurrentFiber();

		//此方法同步完成也会从io完成端口出来
		auto ret = WSARecv(m_handle, &buf, 1, nullptr, &flag, &overlapped, nullptr);
		auto e = WSAGetLastError();
		if (ret != 0 && e != WSA_IO_PENDING) {
			throw Win32SocketException{ static_cast<DWORD>(e) };
		}

		Fiber::GetThis().SwitchMain();


		DWORD count;

		if (WSAGetOverlappedResult(m_handle, &overlapped, &count, false, &flag)) {

			return static_cast<ULONG>(count);
		}
		else {

			throw Win32SocketException{ "Read" };
		}

	}

public:

	TcpSocket() :is_close(false){
		m_handle = Info::CreateIPv4TcpSocket();
		Fiber::GetThis().AddToIoCompletionPort(reinterpret_cast<HANDLE>(m_handle));
	}

	TcpSocket(SOCKET s) :is_close(false), m_handle(s){
		

		Fiber::GetThis().AddToIoCompletionPort(reinterpret_cast<HANDLE>(m_handle));
	}

	auto Write(char* buffer, DWORD len){

		WSABUF buf = {};

		buf.buf  = buffer;

		buf.len= len;

		return this->Write(&buf, 1);
	}


	ULONG Write(WSABUF* buf, DWORD bufCount) {
		this->OnClose_Throw();

		OverLappedEx overlapped = {};

		overlapped.other = GetCurrentFiber();
		
		auto ret = WSASend(m_handle, buf, bufCount, nullptr, 0, &overlapped, nullptr);
	
		auto e = WSAGetLastError();
		
		if (ret != 0 && e != WSA_IO_PENDING) {
			throw Win32SocketException{ static_cast<DWORD>(e) };
		}
		
		Fiber::GetThis().SwitchMain();
	
		DWORD count;
		
		DWORD flag;
		
		if (WSAGetOverlappedResult(m_handle, &overlapped, &count, false, &flag)) {
			
			return static_cast<ULONG>(count);
		}
		else {

			throw Win32SocketException{ "Write send error:", static_cast<DWORD>(WSAGetLastError())};
		}
	}

	

	ULONG Read(char* buffer, ULONG size) {

		return this->Read(buffer, size, 0);
	}

	ULONG Peek(char* buffer, ULONG size) {

		return this->Read(buffer, size, MSG_PEEK);
	}

	auto GetHandle() const {
		
		return m_handle;
	}

	static void Bind(SOCKET handle, const IPEndPoint& endPoint) {

		auto address = endPoint.Get();

		if (SOCKET_ERROR == ::bind(handle, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
			
			throw Win32SocketException{ "bind" };
		}
	}

	static std::shared_ptr<TcpSocket> Connect(const IPEndPoint& endPoint) {
		
		auto handle = std::make_shared<TcpSocket>();

		TcpSocket::Bind(handle->GetHandle(), IPEndPoint{ "0.0.0.0", 0 });

		auto address = endPoint.Get();

		OverLappedEx overlapped = {};

		overlapped.other = GetCurrentFiber();
		
		if (TRUE == Info::GetConnectEx()(handle->GetHandle(), reinterpret_cast<sockaddr*>(&address), sizeof(address), nullptr, 0, nullptr, &overlapped)) {
			WSAExit("connect 同步完成");
			throw Win32SocketException{ "connect 同步完成"};
		}
		else {
			auto value = WSAGetLastError();

			if (value != ERROR_IO_PENDING) {
				throw Win32SocketException{ static_cast<DWORD>(value) };
			}
			else {
				Fiber::GetThis().SwitchMain();

				DWORD count;
				
				DWORD flag;
				
				if (WSAGetOverlappedResult(handle->GetHandle(), &overlapped, &count, false, &flag)) {

					return handle;
				}
				else {

					throw Win32SocketException{ "Connect"};
				}
			}
		}


	}

	void ShutDown() {
		this->OnClose_Throw();
		::shutdown(m_handle, SD_BOTH);
	}

	void OnClose_Throw(){
		if(is_close){
			throw Win32SocketException{"socket is close can not use"};
		}
	}


	void Close(){

		if(is_close==false){

			is_close = true;

			auto isok = ::closesocket(m_handle);

			if(isok == SOCKET_ERROR){
				WSAExit("close socker error");
			}
			
		}

		
	}

	~TcpSocket() {
		

		this->Close();
	}
};



class TcpSocketListen : Delete_Base {
	SOCKET m_handle;

	void CopyOptions(SOCKET source, SOCKET des) {
		
		if (SOCKET_ERROR == setsockopt(des, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char*>(&source), sizeof(source)))
		{
			WSAExit("Copy Options error");
		}
	}


public:

	TcpSocketListen() {

		m_handle = Info::CreateIPv4TcpSocket();

		Fiber::GetThis().AddToIoCompletionPort(reinterpret_cast<HANDLE>(m_handle));
	}

	void Bind(const IPEndPoint& endPoint) {

		TcpSocket::Bind(m_handle, endPoint);
		
	}

	/* auto SetSockOpt(){
		DWORD v = 1;
		auto isok = ::setsockopt(m_handle, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&v), sizeof(v));

		if(isok == SOCKET_ERROR){
			throw Win32SocketException{ "set KEEPALIVE opt error", WSAGetLastError() };
		}
		else{
			Print("set KEEPALIVE opt ok");
		}
	}
 */

	void Listen(int backlog) {
		if (SOCKET_ERROR == ::listen(m_handle, backlog)) {
			
			throw Win32SocketException{ "listen" };
		}
	}

	std::shared_ptr<TcpSocket> Accept() {

		constexpr DWORD ADDRESSLENGTH = sizeof(sockaddr_in) + 16;
		
		constexpr DWORD BUFFERLENGTH = ADDRESSLENGTH * 2;

		auto handle = std::make_shared<TcpSocket>();


		char buffer[BUFFERLENGTH]{};
		
		DWORD length;
		
		OverLappedEx overlapped = {};

		overlapped.other = GetCurrentFiber();
		
		if (TRUE == Info::GetAcceptEx()(m_handle, handle->GetHandle(), buffer, 0, ADDRESSLENGTH, ADDRESSLENGTH, &length, &overlapped)) {
			WSAExit("accept syn over");

			throw Win32SocketException{};
		}
		else {
			auto value = WSAGetLastError();

			if (value != ERROR_IO_PENDING) {
				throw Win32SocketException{ static_cast<DWORD>(value) };
			}
			else {
				Fiber::GetThis().SwitchMain();
				
				DWORD count;
				
				DWORD flag;
				
				if (WSAGetOverlappedResult(m_handle, &overlapped, &count, false, &flag)) {
					
					//TcpSocketListen::CopyOptions(m_handle, handle->GetHandle());

					return handle;

				}
				else {
					
					throw Win32SocketException{ "Accpet" };
				}	
			}	
		}
	}

	
	/* auto SetSockOpt(){

		DWORD v = 0;
		int length = sizeof(v);

		auto isok = ::getsockopt(handle->GetHandle(), SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char *>(&v), &length);

		if (isok == SOCKET_ERROR)
		{
			throw Win32SocketException{"get SO_KEEPALIVE opt error", WSAGetLastError()};
		}
		else
		{
			Print("get SO_KEEPALIVE opt value:", v);
		}

		return handle;
	} */

	~TcpSocketListen() {
		
		::closesocket(m_handle);
	}
};



class TcpSocketListenSync : Delete_Base {
	SOCKET m_handle;

public:

	TcpSocketListenSync() {

		m_handle = Info::CreateIPv4TcpSocket();
	}

	void Bind(const IPEndPoint& endPoint) {

		TcpSocket::Bind(m_handle, endPoint);
		
	}


	void Listen(int backlog) {
		if (SOCKET_ERROR == ::listen(m_handle, backlog)) {
			
			throw Win32SocketException{ "listen" };
		}
	}

	SOCKET Accept() {

		sockaddr_in client;
        int clientsize = sizeof(client);
        auto connct = ::accept(m_handle, (SOCKADDR *)&client, &clientsize);

        if (connct == INVALID_SOCKET)
        {
            Exit("accept socket error", WSAGetLastError());
        }

		return connct;
	}

	
	~TcpSocketListenSync() {
		
		::closesocket(m_handle);
	}
};


class Url {
	
	class Error : public std::exception{
		public:
			Error(){

			}
	};

	uint32_t static GetNumber(uint32_t value) {

		constexpr uint32_t NUMBER_MAP[]{ 0,1,2,3,4,5,6,7,8,9 };

		constexpr uint32_t NUMBER_FIRST = u8'0';

		constexpr uint32_t NUMBER_LAST = u8'9';

		constexpr uint32_t _MAP[]{ 10,11,12,13,14,15 };

		constexpr uint32_t _FIRST = u8'A';

		constexpr uint32_t _LAST = u8'F';

		if (value >= NUMBER_FIRST && value <= NUMBER_LAST) {

			return NUMBER_MAP[value - NUMBER_FIRST];
		}
		else if (value >= _FIRST && value <= _LAST) {

			return _MAP[value - _FIRST];
		}
		else {
			throw Url::Error{};
		}
	}

	mt::mychar static GetCharFrom(const mt::mychar* buffer) {

		auto value_1 = static_cast<uint32_t>(*buffer);

		auto value_2 = static_cast<uint32_t>(*(buffer + 1));

		return static_cast<mt::mychar>((Url::GetNumber(value_1) * 16) + Url::GetNumber(value_2));
	}

	static mt::mystring UrlDecode(const mt::mystring_view& s) {

		constexpr size_t SIZE = 2;

		mt::mystring ret{};

		ret.reserve(s.size());

		auto buffer = s.data();

		auto size = s.size();

		size_t index = 0;

		while (index < size)
		{
			if (buffer[index] == u8'%') {

				index++;

				if ((index + SIZE) <= size) {

					ret.push_back(Url::GetCharFrom(&buffer[index]));

					index += SIZE;
				}
				else {
					throw Url::Error{};
				}
			}
			else {
				
				ret.push_back(buffer[index]);

				index++;
			}
		}


		return (ret);
	}

public:
	static bool UrlDecode(const mt::mystring_view& s, mt::mystring& out_s) {
		try {
			out_s = Url::UrlDecode(s);
			return true;
		}
		catch (Url::Error&) {
			return false;
		}
	}
};



class Number {
public:
	template<typename T>
	static bool Parse(const mt::mystring_view& s, T& out_value) requires(std::is_same_v<T, UINT16> || std::is_same_v<T, UINT32> || std::is_same_v<T, UINT64>)  {
		
		auto first = s.begin();
		auto end = s.end();
		auto res = std::from_chars(first, end, out_value, 10);

		if(res.ec != std::errc{} || res.ptr != end){
			return false;
		}
		else{
			return true;
		}
		

	}


	template<typename T>
	static void ToString(mt::mystring& s, T value) requires(std::is_same_v<T, UINT16> || std::is_same_v<T, UINT32> || std::is_same_v<T, UINT64>) {

		char buff[128];
		auto res = std::to_chars(buff, buff+sizeof(buff), value, 10);

		if(res.ec!=std::errc{}){
			throw ArgumentException("ToString error");
		}
		else{
			auto size = res.ptr - buff;
			s.append(buff, static_cast<size_t>(size));
		}
	}


};






class HttpReqest : Delete_Base {
	
public:
	class FormatException : public std::exception {

	std::string m_message;
public:

	FormatException(std::string message) :m_message(message){

	}


	const char* what() const noexcept override {
		return m_message.c_str();
	}
};

private:
	constexpr static size_t BUFFER_SIZE = 4096;

	//这两个变量目的是为了给map中的view保存缓存生存期
	mt::mystring m_buffer;
	mt::mystring m_firstLine;


	mt::mystring m_path;
	
	std::unordered_map<mt::mystring_view, mt::mystring_view> m_dic;
	std::unordered_map<mt::mystring_view, mt::mystring_view> m_queryArgs;
	static mt::mystring Path(mt::mystring_view s) {

		auto first = s.find(u8' ');

		auto last = s.rfind(u8' ');

		if (first != decltype(s)::npos && first != last) {

			s.remove_suffix(s.size() - last);

			s.remove_prefix(first + 1);

			mt::mystring ret{};

			if (Url::UrlDecode(s, ret)) {
				return ret;
			}
			else {
				throw HttpReqest::FormatException{"url decode error"};
			}
		}
		else {
			throw HttpReqest::FormatException{"find path error"};
		}
	}

	static bool Find(mt::mystring_view& s, mt::mystring_view& out_s) {
		
		auto index =  s.find(MYTEXT("\r\n"));

		if (index == std::remove_reference_t<decltype(s)>::npos) {
		
			return false;
		}
		else if (index == 0) {
			
			s.remove_prefix(index + 2);

			return false;
		}
		else {

			

			out_s = s.substr(0, index);

			s.remove_prefix(index + 2);

			return true;
		}
	}

	static mt::mystring_view TrimSpans(mt::mystring_view s){

		while (true)
		{
			auto a = s.find(MYTEXT(" "));

			if (a != decltype(s)::npos) {
				s.remove_prefix(1);
			}
			else{
				auto b = s.rfind(MYTEXT(" "));
				while (true)
				{
					if (b != decltype(s)::npos) {
						s.remove_suffix(1);
					}
					else{
						return s;
					}
				}
				
			}
		}
	}

	static void AddDic(std::unordered_map<mt::mystring_view, mt::mystring_view>& dic, mt::mystring_view s) {
		
		auto index = s.find(MYTEXT(":"));

		if (index == decltype(s)::npos) {
			throw HttpReqest::FormatException{"find header : error"};
		}
		else {

		
			auto key = s.substr(0, index);
			key = TrimSpans(key);
			auto value = s.substr(index+1);
			value = TrimSpans(value);
			dic.emplace(key, value);
		}
	}


	static bool ParseRange(mt::mystring_view s, std::pair<size_t, std::pair<bool, size_t>>& out_value) {

		constexpr mt::mychar HEAD[] = MYTEXT("bytes=");

		constexpr size_t HEAD_SIZE = sizeof(HEAD) - 1;

		constexpr mt::mychar B = u8'-';

		constexpr size_t B_SIZE = 1;

		auto npos = decltype(s)::npos;

		auto index = s.find(HEAD);

		if (index == npos) {
			return false;
		}
		else {

			s.remove_prefix(index + HEAD_SIZE);

			index = s.find(B);

			if (index == npos) {
				return false;
			}
			else {

				size_t start_range;
				if (!Number::Parse(s.substr(0, index), start_range)) {
					return false;
				}
				else {
					s.remove_prefix(index + B_SIZE);

					size_t end_range;
					if (s.size() == 0 || !Number::Parse(s, end_range)) {
						out_value = std::make_pair(start_range, std::make_pair(false, 0));

						return true;
					}
					else {
						out_value = std::make_pair(start_range, std::make_pair(true, end_range));


						return true;
					}
				}
			}
		}
	}

	static
		mt::mystring_view
		ParseQuery(mt::mystring_view s,
				   std::unordered_map<mt::mystring_view, mt::mystring_view> &dic)
	{

		auto index = s.find(MYTEXT("?"));

		if (index == std::remove_reference_t<decltype(s)>::npos)
		{

			return s.substr(0, s.size());
		}

		mt::mystring_view path = s.substr(0, index);

		s.remove_prefix(index + 1);

		while (true)
		{

			auto index = s.find(MYTEXT("&"));
			mt::mystring_view query_args{};
			if (index == std::remove_reference_t<decltype(s)>::npos)
			{

				query_args = s.substr(0, s.size());
				s.remove_prefix(s.size());
			}
			else
			{
				query_args = s.substr(0, index);
				s.remove_prefix(index + 1);
			}

			if (query_args.size() != 0)
			{
				auto index = query_args.find(MYTEXT("="));

				if (index == std::remove_reference_t<decltype(s)>::npos)
				{

					auto key = query_args.substr(0, query_args.size());

					auto value = mt::mystring_view{};

					dic.emplace(key, value);
				}
				else
				{
					auto key = query_args.substr(0, index);
					query_args.remove_prefix(index + 1);
					auto value = query_args.substr(0, query_args.size());

					dic.emplace(key, value);
				}
			}

			if (s.size() == 0)
			{
				return path;
			}
		}
	}

public:
	
	HttpReqest() : m_buffer(), m_path(), m_firstLine(), m_dic(), m_queryArgs() {

		m_buffer.resize(HttpReqest::BUFFER_SIZE);
	}

	auto& GetDic() const {
		return m_dic;
	}

	auto& GetPath() const {
		return m_path;
	}

	mt::mystring GetValue(const mt::mystring& key){
		
		decltype(auto) dic = this->GetDic();

		auto item = dic.find(key);

		
		if (item == dic.end()) {
			return MYTEXT("");
		}
		else {
			return mt::mystring{ item->second};
		}
	}

	mt::mystring GetQueryValue(const mt::mystring& key){
		auto& dic = this->m_queryArgs;

		auto item = dic.find(key);

		
		if (item == dic.end()) {
			return MYTEXT("");
		}
		else {
			return mt::mystring{ item->second};
		}

	}

	bool GetRange(std::pair<size_t, std::pair<bool, size_t>>& out_value) const {
		
		mt::mystring key{ MYTEXT("Range") };


		decltype(auto) dic = this->GetDic();

		auto item = dic.find(key);

		
		if (item == dic.end()) {
			return false;
		}
		else {
			return HttpReqest::ParseRange(item->second, out_value);
		}
	}

	static std::unique_ptr<HttpReqest> Read(std::shared_ptr<TcpSocket> socket) {
		

		auto ret = std::make_unique<HttpReqest>();

		auto& buffer = ret->m_buffer;
		
		auto& path = ret->m_path;

		auto& dic = ret->m_dic;

		auto& queryArgs = ret->m_queryArgs;

		auto& firstLine = ret->m_firstLine;

		mt::mystring_view view{};
		{
			ULONG length =0;
			auto bufu8 = buffer.data();
			auto buf = reinterpret_cast<char*>(bufu8);
			ULONG canReadSize = static_cast<ULONG>(buffer.size());

			while(true){
				auto n = socket->Read(buf+length, canReadSize);

				length +=n;

				canReadSize-=n;
				view = {bufu8, length};
				auto end = view.find(MYTEXT("\r\n\r\n"));
				if(end != mt::mystring_view::npos){
					if(end+4 == length){
						break;
					}
					else{
						throw HttpReqest::FormatException{"read one request has not use bytes"};
					}
				}
				else{
					if(n == 0){
						throw HttpReqest::FormatException{"not read one request message"};
					}
				}
			}

			
		
		}

		
		//Print(::UTF8::GetMultiByte(::UTF8::GetWideChar(mt::mystring{ view})));
		mt::mystring_view value{};
		
		if (!HttpReqest::Find(view, value)) {
		
			throw HttpReqest::FormatException{"find header line error length:"};
		}
		else {
			//Print(::UTF8::GetMultiByte(::UTF8::GetWideChar(mt::mystring{ value})));
			//path = HttpReqest::Path(value);
			
			firstLine = HttpReqest::Path(value);

			auto pathview = ParseQuery(firstLine, queryArgs);

			path = mt::mystring{pathview};
			
			while (HttpReqest::Find(view, value))
			{
				HttpReqest::AddDic(dic, value);
			}

			return ret;
		}
	}
};


class HttpResponse : Delete_Base {

	mt::mystring m_header;

protected:
	virtual void Send_(std::shared_ptr<TcpSocket> handle, char* header, DWORD size) = 0;
public:

	HttpResponse(size_t statusCode) : m_header() {
		
		m_header.reserve(1024);

		m_header.append(MYTEXT("HTTP/1.1 "));

		Number::ToString(m_header, statusCode);

		if(statusCode == 404){
			m_header.append(MYTEXT(" Not Found\r\n"));
		}
		else{
			m_header.append(MYTEXT(" OK\r\n"));
		}

		m_header.append(MYTEXT("Connection: keep-alive\r\n"));
		m_header.append(MYTEXT("Keep-Alive: timeout=20, max=1000\r\n"));
	}

	void SetContentRange(size_t start, size_t end, size_t size) {
		
		m_header.append(MYTEXT("Content-Range: bytes "));
		
		Number::ToString(m_header, start);
		
		m_header.push_back(u8'-');
		
		Number::ToString(m_header, end);

		m_header.push_back(u8'/');

		Number::ToString(m_header, size);

		m_header.append(MYTEXT("\r\n"));
	}

	void SetContentLength(size_t size) {
		m_header.append(MYTEXT("Content-Length: "));

		Number::ToString(m_header, size);

		m_header.append(MYTEXT("\r\n"));
	}

	void Set(const mt::mystring& key, const mt::mystring& value) {
		m_header.append(key).append(MYTEXT(": ")).append(value).append(MYTEXT("\r\n"));
	}

	
	void SetContentType(const std::wstring& s) {

		decltype(auto) map = Info::GetContentTypeMap();

		auto item = map.find(s);

		m_header.append(MYTEXT("Content-Type: "));

		if (item == map.end()) {
			m_header.append(MYTEXT("application/octet-stream"));
		}
		else {

			m_header.append(item->second);

		}

		m_header.append(MYTEXT("\r\n"));
	}
	



	void Send(std::shared_ptr<TcpSocket> handle) {
	
		m_header.append(MYTEXT("\r\n"));

		auto buf = reinterpret_cast<char*>(m_header.data());

		auto size = ::Integer_cast<size_t, DWORD>(m_header.size());

		this->Send_(handle, buf, size);
	
	}

	virtual ~HttpResponse()
	{

	}
};

class HttpResponseStrContent : public HttpResponse {

	mt::mystring m_str;


protected:
	void Send_(std::shared_ptr<TcpSocket> handle, char* header, DWORD size) override {
		

		WSABUF bufArray[2]{};

		bufArray[0].buf = header;
		bufArray[0].len = size;

		bufArray[1].buf =  reinterpret_cast<char*>(m_str.data());
		bufArray[1].len = ::Integer_cast<size_t, DWORD>(m_str.size());

		handle->Write(bufArray, 2);
	}
public:

	constexpr static auto JSON_TYPE = MYTEXT("application/json");

	constexpr static auto HTML_TYPE = MYTEXT("text/html; charset=utf-8");

	HttpResponseStrContent(size_t statusCode, const std::wstring& s) : HttpResponseStrContent(statusCode, UTF8::GetUTF8ToString(s), HTML_TYPE) {

	}

	HttpResponseStrContent(size_t statusCode, mt::mystring&& s, const mt::mystring& type) : HttpResponse(statusCode), m_str(s) {

		this->Set(MYTEXT("Content-Type"), type);
		this->SetContentLength(m_str.size());

	}
};




class HttpResponse404 : public HttpResponse{


public:

	HttpResponse404() : HttpResponse(404){
		this->SetContentLength(0);

	}

protected:
	void Send_(std::shared_ptr<TcpSocket> handle, char* header, DWORD size)  override {
		
		WSABUF buf ={};

		buf.buf = header;

		buf.len = size;

		handle->Write(&buf, 1);
	}

};

class HttpResponseRangeContent : public HttpResponse {

	//constexpr static size_t BAO_LIU_ZI_JIE_COUNT = 1024*1024*8;

	//constexpr static size_t MAX_SEND_LENGTH = std::numeric_limits<int32_t>::max()-BAO_LIU_ZI_JIE_COUNT;
	//constexpr static size_t MAX_SEND_LENGTH = 5112660345;

protected:
	size_t m_fileSize;

	size_t m_start_range;

	size_t m_end_range;

	size_t m_length;
	
	void Set() {
		this->SetContentLength(m_length);

		this->SetContentRange(m_start_range, m_end_range, m_fileSize);
	}


public:

	HttpResponseRangeContent(size_t contentSize)
		: HttpResponse(206), m_fileSize(contentSize) {
		
		
	}



	
	void SetRange(size_t start, size_t end) {

		if(end >= m_fileSize){
			throw ArgumentException{"request set renge end > fileSize"};
		}

		if(start> end){
			throw ArgumentException{"request set renge start > end"};
		}

		auto length = (end - start) + 1;

	


		//Print("start:", start, "end:", end, "length:", length, "v:",v, "fileSize:", m_fileSize);
		m_start_range = start;

		m_end_range = end;
		
		
		

		m_length = length;

		this->Set();
	}

	void SetRange(size_t start) {

		this->SetRange(start, m_fileSize - 1);

	}

	void SetRange() {

		this->SetRange(0, m_fileSize - 1);
	}

	void SetRangeFromRequest(const HttpReqest& req){
		std::pair<size_t, std::pair<bool, size_t>> range;

		if (req.GetRange(range)) {
			
			if (range.second.first) {
				this->SetRange(range.first, range.second.second);
			}
			else {
				this->SetRange(range.first);
			}

		}
		else {

		
			this->SetRange();

		}
	}

};



class  HttpResponseFileContent : public HttpResponseRangeContent{
	
	
	std::unique_ptr<CreateReadOnlyFile> m_file;


	static std::wstring GetName(const std::wstring& path) {
		auto index = path.rfind(L'.');

		if (index == std::remove_reference_t< decltype(path)>::npos) {
			return std::wstring{};
		}
		else {
			return path.substr(index, path.size() - index);
		}
	}


protected:
	void Send_(std::shared_ptr<TcpSocket> handle, char* header, DWORD size) override {

		
		handle->Write(header, size);

		
		const uint32_t oneSendCount= 65536;
		char BUF[oneSendCount];
		MyFunc::CopyTo(
			m_start_range,
			m_length,
			oneSendCount,
			[&file = m_file, &BUF](size_t offset, char** buf_p, uint32_t count){
				auto i = file->Read(BUF, count, offset);

				*buf_p=BUF;

				return i;
			},
			[&handle](char* buf, uint32_t count){
				return handle->Write(buf, count);
			});
	}

public:

	HttpResponseFileContent(const std::wstring& path)
		: HttpResponseRangeContent(0), m_file() {
		
		m_file = std::make_unique<CreateReadOnlyFile>(path);

		m_fileSize = Integer_cast<LONGLONG, size_t>( m_file->GetSize());

		this->SetContentType(HttpResponseFileContent::GetName(path));

	}

};


class HttpResponseBufferContent:public HttpResponseRangeContent{

private:
	std::shared_ptr<std::vector<byte>> m_buf;


protected:
	void Send_(std::shared_ptr<TcpSocket> handle, char* header, DWORD size) override {
		
		

		handle->Write(header, size);
		
		
		const uint32_t oneSendCount = 65536;
			
		//char BUF[oneSendCount];
		MyFunc::CopyTo(
		m_start_range,
		m_length,
		oneSendCount,	
		[&databuf= *m_buf](size_t offset, char** buf_p, uint32_t count){

			*buf_p = (char*)(databuf.data()+offset);

			//Print("run");
			//CopyMemory(BUF, databuf.data()+offset, count);
			
			return count;

		},
		[&soc= handle](char* buf, uint32_t count){
			return soc->Write(buf, count);
		
		});
		
	}
public:

	HttpResponseBufferContent(const std::wstring& exName,std::shared_ptr<std::vector<byte>> buf) : HttpResponseRangeContent(0),m_buf(buf) {

		m_fileSize = m_buf->size();

		this->SetContentType(exName);
		
	}

};




class EnumFileFolder : Delete_Base {


public:

	class Data {
		WIN32_FIND_DATAW m_data;

	public:
		Data() : m_data() {

		}

		auto* Get() {
			return &m_data;
		}

		bool IsFolder() {
			return 0 != (m_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		}

		const wchar_t* Path() const {
			return  m_data.cFileName;
		}

		size_t Size() {

			size_t n = MAXDWORD;

			n += 1;

			size_t high = m_data.nFileSizeHigh;

			size_t low = m_data.nFileSizeLow;

			return (high * n) + low;
		}
	};

private:
	EnumFileFolder::Data m_data;

	HANDLE m_handle;

	bool m_isFirst;

	static bool IsThrow(DWORD error) {
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES) {
			return false;
		}
		else {
			throw Win32SysteamException{ error };
		}

	}

public:

	EnumFileFolder(const std::wstring& path) {

		m_handle = ::FindFirstFileW(path.c_str(), m_data.Get());

		if (INVALID_HANDLE_VALUE == m_handle) {

			EnumFileFolder::IsThrow(GetLastError());

			m_isFirst = false;
		}
		else {
			m_isFirst = true;
		}
	}

	bool Get(EnumFileFolder::Data& out_data) {

		if (m_isFirst) {

			m_isFirst = false;

			out_data = m_data;

			return true;
		}
		else {

			if (FindNextFileW(m_handle, out_data.Get())) {
				return true;
			}
			else {
				return EnumFileFolder::IsThrow(GetLastError());
			}
		}
	}

	~EnumFileFolder()
	{
		::FindClose(m_handle);
	}

};



class File {
public:

	class IsFileIsFolder {
		bool m_isFolder;
		bool m_isFile;

	public:
		IsFileIsFolder(bool isFolder, bool isFile) : m_isFolder(isFolder), m_isFile(isFile) {}

		bool IsFile() {
			return m_isFile;
		}

		bool IsFolder() {
			return m_isFolder;
		}
	};


	static IsFileIsFolder IsFileOrFolder(const std::wstring& path) {
		auto value = GetFileAttributesW(path.c_str());

		if (value == INVALID_FILE_ATTRIBUTES) {
			auto e = GetLastError();

			if (e == ERROR_FILE_NOT_FOUND) {
				return IsFileIsFolder{ false, false };
			}
			else {
				throw Win32SysteamException{ e };
			}
		}
		else {
			if (0 == (value & FILE_ATTRIBUTE_DIRECTORY)) {
				return IsFileIsFolder{ false, true };
			}
			else {
				return IsFileIsFolder{ true, false };
			}
		}
	}

};



class Html {
private:
	std::wstring m_file;

	std::wstring m_folder;


	void Add(bool isFolder, std::wstring& s, const std::wstring& path, const std::wstring& name) {
		
		s.append(L"<li><a href=\"");
		std::wstring encodeUrl = ::UTF8::UrlEncode(path.c_str());
		s.append(encodeUrl);

		if (isFolder) {

			s.append(L"/\">");

		}
		else {

			s.append(L"\">");

		}

		s.append(name);

		s.append(L"</a></li>");
	}
	

public:

	Html():m_file(), m_folder(){

	}

	void Add(bool isFolder,  const std::wstring& path, const std::wstring& name){
		if(isFolder){
			this->Add(isFolder, m_folder, path, name);
		}
		else{
			this->Add(isFolder, m_file, path, name);
		}
	}

	std::wstring GetHtml() {

	
		std::wstring ret{};
		
		ret.append(L"<!DOCTYPE html><html lang=\"zh-cn\" xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta charset=\"utf-8\" /><title>文件和文件</title></head><body><div><div><ul>");
		
		ret.append(m_folder);
		
		ret.append(L"</ul></div><div><ul>");
		
		ret.append(m_file);
		
		ret.append(L"</ul></div></div></body></html>");

		return ret;
	}

};
#endif // !_MYIO