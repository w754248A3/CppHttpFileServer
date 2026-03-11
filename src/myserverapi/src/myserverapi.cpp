#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <ios>
#include <istream>
#include <minwindef.h>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <winnt.h>

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
#include "mypublicapi.h"
#include "myserverapi.h"



void WSAExit(const std::wstring& message) {
	MyWin32Out::MyWin32Out::Exit(message, WSAGetLastError());
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
				count = static_cast<uint32_t>(needCopyCount);
			}

			char* buf=nullptr;
			uint32_t redCount = readfunc(readOffset, &buf, count);

			if(redCount ==0 || buf==nullptr){
				
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

	Win32SocketException(const std::wstring& message) : Win32SysteamException(message, static_cast<DWORD>(WSAGetLastError())) {

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




class Number {
public:
	template<typename T>
	static bool Parse(const mt::mystring_view& s, T& out_value, int base = 10) requires(std::is_same_v<T, unsigned char> || std::is_same_v<T, UINT16> || std::is_same_v<T, UINT32> || std::is_same_v<T, UINT64>)  {
		
		auto first = s.begin();
		auto end = s.end();
		auto res = std::from_chars(first, end, out_value, base);

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


class HttpHeaderMap{

	public:

	enum Headers {

		Range,
		ContentType,
		ContentLength,
		Connection,



	};

	static auto& GetHttpHeaderMap() {

		static std::unordered_map<mt::mystring_view, HttpHeaderMap::Headers> map{};

		return map;
	}

	static auto to_lower(mt::mystring_view source, std::span<char, 64>  dest) {

		if(source.size()> dest.size()){
			throw ArgumentException("to_lower dest size is small");
		}

		for (mt::mystring_view::size_type i = 0;i < source.size(); i++ ) {
			
			if(source[i] >= 'A' && source[i] <= 'Z'){
				dest[i] = static_cast<char>(source[i] + 32);
			}
			else{
				dest[i] = source[i];
			}

		}

		return mt::mystring_view{ dest.data(), source.size() };

		
	}

	static void Set(std::unordered_map<HttpHeaderMap::Headers, mt::mystring_view>& dic, mt::mystring_view key, mt::mystring_view value){

		decltype(auto) map = GetHttpHeaderMap();

		std::array<char, 64> buf{};

		auto low_key = to_lower(key,buf);

		auto iter = map.find(low_key);

		if(iter != map.end()){

			dic.emplace(iter->second, value);
			
		}
		else{
			
		}

	}

	static void InitializationMap() {

		decltype(auto) map = GetHttpHeaderMap();


		map.emplace(MYTEXT("range"), HttpHeaderMap::Headers::Range);
		map.emplace(MYTEXT("content-type"), HttpHeaderMap::Headers::ContentType);
		map.emplace(MYTEXT("content-length"), HttpHeaderMap::Headers::ContentLength);
		map.emplace(MYTEXT("connection"), HttpHeaderMap::Headers::Connection);

		
	}




};




class Info {

public:
	static auto CreateIPv4TcpSocket() {
		auto handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (handle == INVALID_SOCKET) {
			WSAExit(L"create socket error");

			return handle;
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
			WSAExit(L"get function address error");

			return functionAddress;
		}
		else {
			return functionAddress;
		}
	}

	

	inline static LPFN_ACCEPTEX s_acceptex;

	inline static LPFN_CONNECTEX s_connectex;

	static void InitializationWSA() {
		WSADATA data;

		auto value = WSAStartup(MAKEWORD(2, 2), &data);

		if (value != 0) {
			MyWin32Out::MyWin32Out::Exit(L"Initialization error", value);
		}

		s_acceptex = Info::GetFunctionAddress<LPFN_ACCEPTEX>(WSAID_ACCEPTEX);

		s_connectex = Info::GetFunctionAddress<LPFN_CONNECTEX>(WSAID_CONNECTEX);

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

		HttpHeaderMap::InitializationMap();

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
public:


	static auto MyGetCurrentFiber(){
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
// 你的问题代码写在这里
		auto h = ::GetCurrentFiber();
#pragma GCC diagnostic pop
	
		return h;
	}
	
	
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
			MyWin32Out::Exit(L"fiber * is null");
			std::unreachable();
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
			MyWin32Out::Exit(L"create Io Completion Port error");

			return handle;
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
			MyWin32Out::Print(L"fiber throw socket throw");

			MyWin32Out::Exit(e.what());
		}
		catch (Win32SysteamException& e) {
			MyWin32Out::Print(L"fiber throw system throw");
			MyWin32Out::Exit(e.what());
		}
		catch (std::exception& e) {
			MyWin32Out::Print(L"fiber throw exception throw");
			auto s = UTF8::GetWideCharFromUTF8(e.what());
			MyWin32Out::Exit(s);
		}
		catch (...) {
			MyWin32Out::Exit(L"fiber throw error");
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

				 Fiber::GetThis().PostToIoCompletionPort(IOPortFlag::FiberDelete, Fiber::MyGetCurrentFiber());

			}
			else {
				queue.push_back(Fiber::MyGetCurrentFiber());
			}

			Fiber::GetThis().SwitchMain();
		}
		
	}

public:

	void AddToIoCompletionPort(HANDLE fileHandle) {
		auto handle = ::CreateIoCompletionPort(fileHandle, m_io_over_port, static_cast<ULONG_PTR>(IOPortFlag::FiberSwitch), 0);
		if (handle == nullptr) {
			MyWin32Out::Exit(L"add Io Completion Port error");
		}
	}

	void PostToIoCompletionPort(IOPortFlag flag, LPVOID value) {
		if (0 == ::PostQueuedCompletionStatus(m_io_over_port, 0, static_cast<ULONG_PTR>(flag), static_cast<LPOVERLAPPED>(value))) {
			MyWin32Out::Exit(L"post io Completion Port error");
		}
	}

	

	void Convert() {
		auto handle = ::ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);

		if (handle == nullptr) {
			MyWin32Out::Exit(L"Convert To Fiber Error");
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
				MyWin32Out::Exit(L"Create Fiber Error");
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
		if (fiber == Fiber::MyGetCurrentFiber()) {
			MyWin32Out::Exit(L"Switch Fiber error");
		}

		::SwitchToFiber(fiber);
	}

	void Delete(LPVOID fiber) {
		if (fiber == m_main_fiber) {
			MyWin32Out::Exit(L"delete fiber error");

		}

		::DeleteFiber(fiber);
	}

	template <typename... TS>
	void Start(Fiber::FiberFuncType<TS...> func, TS... value)
	{
		
		if(Info::IsCallInitialization() == false){
			MyWin32Out::Exit(L"can not call Initialization");
		}

		Fiber::s_value= this;

		Fiber::Convert();

		if(Fiber::s_value != this){
			MyWin32Out::Exit(L"convert fiber thread local data not eq");
		}

		Fiber::Create(func, value...);

		std::array<OVERLAPPED_ENTRY, 32> buffer{};

		DWORD count;
		
		while (true)
		{
			
		
			auto res = GetQueuedCompletionStatusEx(Fiber::GetPortHandle(), buffer.data(), static_cast<ULONG>(buffer.size()), &count, INFINITE, true);
			
			if (TRUE !=res)
			{
				MyWin32Out::Exit(L"get io error");
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
						MyWin32Out::Exit(L"can not define Fiber flag");
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
			auto error = GetLastError();
			throw Win32SysteamException{L"CreateReadOnlyFile:"+path, error};
		
		}
		Fiber::GetThis().AddToIoCompletionPort(reinterpret_cast<HANDLE>(m_handle));
	}

	auto GetSize() {
		LARGE_INTEGER size;
		if (GetFileSizeEx(m_handle, &size)) {
			return size.QuadPart;
		}
		else {
			auto error = GetLastError();
			throw Win32SysteamException{L"GetSize", error};
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
		
		
		overlapped.other = Fiber::MyGetCurrentFiber();
		
		auto ret = ::ReadFile(m_handle, buf, size, nullptr, &overlapped);
		auto e = GetLastError();
		if(ret != 0 || e != ERROR_IO_PENDING){
			
			throw Win32SysteamException{L"read file error:", e};
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

				throw Win32SocketException{L"Read file over error:", e };
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
	bool is_close;
	SOCKET m_handle;
	
	ULONG Read(char* buffer, ULONG size, DWORD flag) {
		this->OnClose_Throw();

		WSABUF buf = {};

		buf.buf = buffer;

		buf.len = size;

		OverLappedEx overlapped = {};

		overlapped.other = Fiber::MyGetCurrentFiber();

		//此方法同步完成也会从io完成端口出来
		auto ret = WSARecv(m_handle, &buf, 1, nullptr, &flag, &overlapped, nullptr);
		auto e = WSAGetLastError();
		if (ret != 0 && e != WSA_IO_PENDING) {
			throw Win32SocketException{ L"Read WSARecv", static_cast<DWORD>(e) };
		}

		Fiber::GetThis().SwitchMain();


		DWORD count;

		if (WSAGetOverlappedResult(m_handle, &overlapped, &count, false, &flag)) {

			return static_cast<ULONG>(count);
		}
		else {
			auto e = WSAGetLastError();
			throw Win32SocketException{ L"Read", static_cast<DWORD>(e) };
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

		overlapped.other = Fiber::MyGetCurrentFiber();
		
		auto ret = WSASend(m_handle, buf, bufCount, nullptr, 0, &overlapped, nullptr);
	
		auto e = WSAGetLastError();
		
		if (ret != 0 && e != WSA_IO_PENDING) {
			throw Win32SocketException{L"WSASend", static_cast<DWORD>(e) };
		}
		
		Fiber::GetThis().SwitchMain();
	
		DWORD count;
		
		DWORD flag;
		
		if (WSAGetOverlappedResult(m_handle, &overlapped, &count, false, &flag)) {
			
			return static_cast<ULONG>(count);
		}
		else {
			auto e = WSAGetLastError();
			throw Win32SocketException{ L"Write send error:", static_cast<DWORD>(e)};
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
			
			throw Win32SocketException{ L"bind" };
		}
	}

	static std::shared_ptr<TcpSocket> Connect(const IPEndPoint& endPoint) {
		
		auto handle = std::make_shared<TcpSocket>();

		TcpSocket::Bind(handle->GetHandle(), IPEndPoint{ "0.0.0.0", 0 });

		auto address = endPoint.Get();

		OverLappedEx overlapped = {};

		overlapped.other = Fiber::MyGetCurrentFiber();
		
		if (TRUE == Info::GetConnectEx()(handle->GetHandle(), reinterpret_cast<sockaddr*>(&address), sizeof(address), nullptr, 0, nullptr, &overlapped)) {
			WSAExit(L"connect 同步完成");
			throw Win32SocketException{L"connect 同步完成"};
		}
		else {
			auto value = WSAGetLastError();

			if (value != ERROR_IO_PENDING) {
				throw Win32SocketException{L"GetConnectEx", static_cast<DWORD>(value) };
			}
			else {
				Fiber::GetThis().SwitchMain();

				DWORD count;
				
				DWORD flag;
				
				if (WSAGetOverlappedResult(handle->GetHandle(), &overlapped, &count, false, &flag)) {

					return handle;
				}
				else {

					throw Win32SocketException{ L"Connect"};
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
			throw Win32SocketException{L"socket is close can not use"};
		}
	}


	void Close(){

		if(is_close==false){

			is_close = true;

			auto isok = ::closesocket(m_handle);

			if(isok == SOCKET_ERROR){
				WSAExit(L"close socker error");
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
			WSAExit(L"Copy Options error");
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
			MyWin32Out::Print("set KEEPALIVE opt ok");
		}
	}
 */

	void Listen(int backlog) {
		if (SOCKET_ERROR == ::listen(m_handle, backlog)) {
			
			throw Win32SocketException{ L"listen" };
		}
	}

	std::shared_ptr<TcpSocket> Accept() {

		constexpr DWORD ADDRESSLENGTH = sizeof(sockaddr_in) + 16;
		
		constexpr DWORD BUFFERLENGTH = ADDRESSLENGTH * 2;

		auto handle = std::make_shared<TcpSocket>();


		char buffer[BUFFERLENGTH]{};
		
		DWORD length;
		
		OverLappedEx overlapped = {};

		overlapped.other = Fiber::MyGetCurrentFiber();
		
		if (TRUE == Info::GetAcceptEx()(m_handle, handle->GetHandle(), buffer, 0, ADDRESSLENGTH, ADDRESSLENGTH, &length, &overlapped)) {
			WSAExit(L"accept syn over");

			throw Win32SocketException{L"GetAcceptEx",};
		}
		else {
			auto value = WSAGetLastError();

			if (value != ERROR_IO_PENDING) {
				throw Win32SocketException{L"GetAcceptEx", static_cast<DWORD>(value) };
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
					
					throw Win32SocketException{ L"Accpet" };
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
			MyWin32Out::Print("get SO_KEEPALIVE opt value:", v);
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
			
			throw Win32SocketException{ L"listen" };
		}
	}

	SOCKET Accept() {

		sockaddr_in client;
        int clientsize = sizeof(client);
        auto connct = ::accept(m_handle, reinterpret_cast<SOCKADDR *>(&client), &clientsize);

        if (connct == INVALID_SOCKET)
        {
            MyWin32Out::Exit(L"accept socket error", WSAGetLastError());
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

	mt::mychar static GetCharFrom(const mt::mystring_view str) {
		unsigned char v;
		if(Number::Parse(str, v, 16)){
			return static_cast<mt::mychar>(v);
		}
		else{
			throw Error{};
		}
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

					mt::mystring_view str{&buffer[index], SIZE};
					ret.push_back(Url::GetCharFrom(str));

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


class UrlEncode{
    
    static bool is_valid_utf8(const std::string& str) noexcept {
        int expected = 0;
        for (const char s_c: str) {
            const unsigned char c = static_cast<unsigned char>(s_c);
            if (expected > 0) {
                if ((c & 0xC0) != 0x80) return false;
                --expected;
            } else {
                if ((c & 0x80) == 0x00) continue;
                else if ((c & 0xE0) == 0xC0) expected = 1;
                else if ((c & 0xF0) == 0xE0) expected = 2;
                else if ((c & 0xF8) == 0xF0) expected = 3;
                else return false;
            }
        }
        return expected == 0;
    }


	static bool is_no_need_encode(char c){
		return c == '/' || c == '?' || c =='&' || c=='=' || c =='.' ||
		(c >= 'a' && c <= 'z')||
		(c >= 'A' && c <= 'Z')||
		(c >= '0' && c <= '9');
	}

public:
    static bool url_encode(std::string& out,  const std::string& input) {
        if (!is_valid_utf8(input)) {
            return false;
        }

        for (const char s_c : input) {
            

			if(is_no_need_encode(s_c)){
				
				out.push_back(s_c);
				
			}
			else{
				const unsigned char c = static_cast<unsigned char>(s_c);
				char hex[10];
				auto res = std::snprintf(hex, sizeof(hex), "%%%02X", c);
			
				out.append(hex, static_cast<size_t>(res));
			}
            
        }

        return true;
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
	
	std::unordered_map<HttpHeaderMap::Headers, mt::mystring_view> m_dic;
	std::unordered_map<mt::mystring_view, mt::mystring_view> m_queryArgs;


	mt::Method m_method;

	static mt::Method ParseMethod(mt::mystring_view s) {

		if (s == MYTEXT("GET")) {
			return mt::Method::GET;
		}
		else if (s == MYTEXT("HEAD")) {
			return mt::Method::HEAD;
		}
		else if (s == MYTEXT("POST")) {
			return mt::Method::POST;
		}
		else {
			throw HttpReqest::FormatException{"find method error"};
		}
	}

	static void ParsePathAndMethod(mt::mystring_view s, mt::mystring& out_path, mt::Method& out_method) {

		auto first = s.find(u8' ');

		auto last = s.rfind(u8' ');

		if (first != decltype(s)::npos && first != last) {

			auto method_str = s.substr(0, first);


			out_method = ParseMethod(method_str);

			s.remove_suffix(s.size() - last);

			s.remove_prefix(first + 1);

			mt::mystring ret{};
			if (Url::UrlDecode(s, ret)) {
				out_path = ret;
				return;
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

	

	static void AddDic(std::unordered_map<HttpHeaderMap::Headers, mt::mystring_view>& dic, mt::mystring_view s) {
		
		auto index = s.find(MYTEXT(":"));

		if (index == decltype(s)::npos) {
			throw HttpReqest::FormatException{"find header : error"};
		}
		else {

		
			auto key = s.substr(0, index);
			key = TrimSpans(key);
			auto value = s.substr(index+1);
			value = TrimSpans(value);
			HttpHeaderMap::Set(dic, key, value);
			
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
	
	HttpReqest() : m_buffer(), m_firstLine(), m_path(), m_dic(), m_queryArgs() {

		m_buffer.resize(HttpReqest::BUFFER_SIZE);
	}

	auto& GetDic() const {
		return m_dic;
	}

	auto& GetMethod() const {
		return m_method;
	}

	auto& GetPath() const {
		return m_path;
	}

	mt::mystring GetValue(HttpHeaderMap::Headers key) {
		
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
		
		

		decltype(auto) dic = this->GetDic();

		auto item = dic.find(HttpHeaderMap::Headers::Range);

		
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

		
		//MyWin32Out::Print(::UTF8::GetMultiByte(::UTF8::GetWideCharFromUTF8(mt::mystring{ view})));
		mt::mystring_view value{};
		
		if (!HttpReqest::Find(view, value)) {
		
			throw HttpReqest::FormatException{"find header line error length:"};
		}
		else {
			//MyWin32Out::Print(::UTF8::GetMultiByte(::UTF8::GetWideChar(mt::mystring{ value})));
			//path = HttpReqest::Path(value);
			
			HttpReqest::ParsePathAndMethod (value, firstLine, ret->m_method);

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

class ResponseFunc{
private:

	static std::wstring GetName(const std::wstring& path) {
		auto index = path.rfind(L'.');

		if (index == std::remove_reference_t< decltype(path)>::npos) {
			return std::wstring{};
		}
		else {
			return path.substr(index, path.size() - index);
		}
	}

	static void SetContentLength(mt::mystring& header, size_t size) {
		header.append(MYTEXT("Content-Length: "));

		Number::ToString(header, size);

		header.append(MYTEXT("\r\n"));
	}

	static void SetContentRange(mt::mystring& header, size_t start, size_t end, size_t size) {
		
		header.append(MYTEXT("Content-Range: bytes "));
		
		Number::ToString(header, start);
		
		header.push_back(u8'-');
		
		Number::ToString(header, end);

		header.push_back(u8'/');

		Number::ToString(header, size);

		header.append(MYTEXT("\r\n"));
	}


	static size_t CheckRangeReturnLength(mt::mystring& header, size_t start, size_t end, size_t fileSize) {

		if(end >= fileSize){
			throw ArgumentException{"request set renge end > fileSize"};
		}

		if(start> end){
			throw ArgumentException{"request set renge start > end"};
		}

		auto length = (end - start) + 1;

	
		
		ResponseFunc::SetContentRange(header, start, end, fileSize);

		return length;
	}

	static void SetContentType(mt::mystring& m_header, const std::wstring& s) {

		decltype(auto) map = Info::GetContentTypeMap();

		auto sv = s;

		for (wchar_t& ch : sv) {
			if (ch >= L'A' && ch <= L'Z') {
				ch = ch + (L'a' - L'A');
			}
    	}

		auto item = map.find(sv);

		m_header.append(MYTEXT("Content-Type: "));

		if (item == map.end()) {
			m_header.append(MYTEXT("application/octet-stream"));
		}
		else {

			m_header.append(item->second);

		}

		m_header.append(MYTEXT("\r\n"));
	}

	static void SetPublicHeader(mt::mystring& m_header ) {
		m_header.append(MYTEXT("Connection: keep-alive\r\n"));
		m_header.append(MYTEXT("Keep-Alive: timeout=20, max=1000\r\n"));
	}

	static void SetRangeAcceptedHeader(mt::mystring& m_header) {
		m_header.append(MYTEXT("Accept-Ranges: bytes\r\n"));
	}

	static void SendHeader(std::shared_ptr<TcpSocket> handle, mt::mystring& header) {
		
		
		header.append(MYTEXT("\r\n"));
		//MyWin32Out::Print(::UTF8::GetMultiByte(::UTF8::GetWideCharFromUTF8(mt::mystring{ header})));
		auto buf = header.data();

		auto size = ::Integer_cast<size_t, DWORD>(header.size());

		handle->Write(buf, size);
	}

	
	static void LoopSendFile(std::shared_ptr<TcpSocket> handle, CreateReadOnlyFile& fileHandle, size_t start_range, size_t length, bool is_Inverted_bits) {
		
		const uint32_t oneSendCount= 65536;
		char BUF[oneSendCount];
		MyFunc::CopyTo(
			start_range,
			length,
			oneSendCount,
			[&file = fileHandle, &BUF, &is_Inverted_bits](size_t offset, char** buf_p, uint32_t count){
				auto i = file.Read(BUF, count, offset);

				if(is_Inverted_bits){

					for (decltype(i) index = 0; index < i; index++) {
						BUF[index] = static_cast<char>(~(static_cast<uint8_t>(BUF[index])));
					}
				}

				*buf_p=BUF;

				return i;
			},
			[&handle](char* buf, uint32_t count){
				return handle->Write(buf, count);
			});
	}

	static void LoopSendBuffer(std::shared_ptr<TcpSocket> handle, std::shared_ptr<std::vector<byte>> buf, size_t start_range, size_t length, bool is_Inverted_bits) {
		
		const uint32_t oneSendCount = 65536;
			
		char BUF[oneSendCount];
		MyFunc::CopyTo(
		start_range,
		length,
		oneSendCount,	
		[&databuf= *buf, &BUF](size_t offset, char** buf_p, uint32_t count){

			//MyWin32Out::Print("run");
			CopyMemory(BUF, databuf.data()+offset, count);
			
			*buf_p= BUF;

			return count;

		},
		[&soc= handle, &is_Inverted_bits](char* buf, uint32_t count){
			
			
			if(is_Inverted_bits){

				for (decltype(count) index = 0; index < count; index++) {
					buf[index] = static_cast<char>(~(static_cast<uint8_t>(buf[index])));
				}
			}
			
			return soc->Write(buf, count);
		
		});
	}

public:

	static void SendFile(const std::wstring& filePath, std::shared_ptr<TcpSocket> handle, const HttpReqest& request, bool is_Inverted_bits){
		
		bool isHeadMethod = request.GetMethod() == mt::Method::HEAD;

		CreateReadOnlyFile fileHandle{filePath};
		const auto fileSize = Integer_cast<LONGLONG, size_t>(fileHandle.GetSize());
		
		const auto fileExName = GetName(filePath);

		mt::mystring m_header{};
		m_header.reserve(1024);

		m_header.append(MYTEXT("HTTP/1.1 "));

		std::pair<size_t, std::pair<bool, size_t>> range;
		
		if(request.GetRange(range)){
			m_header.append(MYTEXT("206 Partial Content\r\n"));
		
			size_t start_range = range.first;
			size_t length = range.second.first?
				ResponseFunc::CheckRangeReturnLength(m_header, range.first, range.second.second, fileSize):
				ResponseFunc::CheckRangeReturnLength(m_header, range.first, fileSize - 1, fileSize);

			ResponseFunc::SetContentLength(m_header, length);
			ResponseFunc::SetContentType( m_header, fileExName);
			ResponseFunc::SetRangeAcceptedHeader(m_header);
			ResponseFunc::SetPublicHeader(m_header);
	
			ResponseFunc::SendHeader(handle, m_header);
			if(isHeadMethod){
				return;
			}
			
			ResponseFunc::LoopSendFile(handle, fileHandle, start_range, length, is_Inverted_bits);

		}
		else{
			m_header.append(MYTEXT("200 OK\r\n"));

			size_t start_range = 0;

			size_t length = fileSize;

			ResponseFunc::SetContentLength(m_header, length);

			ResponseFunc::SetPublicHeader(m_header);

			ResponseFunc::SetContentType( m_header, fileExName);

			ResponseFunc::SetRangeAcceptedHeader(m_header);


			ResponseFunc::SendHeader(handle, m_header);

			if(isHeadMethod){
				return;
			}
			
			ResponseFunc::LoopSendFile(handle, fileHandle, start_range, length, is_Inverted_bits);
		}

	}

	static void SendBuffer(const std::wstring& fileExName,std::shared_ptr<std::vector<byte>> buf, std::shared_ptr<TcpSocket> handle, const HttpReqest& request, bool is_Inverted_bits){
		
	
		const auto fileSize = buf->size();
		
		mt::mystring m_header{};
		m_header.reserve(1024);

		m_header.append(MYTEXT("HTTP/1.1 "));

		std::pair<size_t, std::pair<bool, size_t>> range;
		
		if(request.GetRange(range)){
			m_header.append(MYTEXT("206 Partial Content\r\n"));
		
			size_t start_range = range.first;
			size_t length = range.second.first?
				ResponseFunc::CheckRangeReturnLength(m_header, range.first, range.second.second, fileSize):
				ResponseFunc::CheckRangeReturnLength(m_header, range.first, fileSize - 1, fileSize);

			ResponseFunc::SetContentLength(m_header, length);
			ResponseFunc::SetContentType( m_header, fileExName);
			ResponseFunc::SetRangeAcceptedHeader(m_header);
			ResponseFunc::SetPublicHeader(m_header);
	
			ResponseFunc::SendHeader(handle, m_header);

			ResponseFunc::LoopSendBuffer(handle, buf, start_range, length, is_Inverted_bits);

		}
		else{
			m_header.append(MYTEXT("200 OK\r\n"));

			size_t start_range = 0;

			size_t length = fileSize;

			ResponseFunc::SetContentLength(m_header, length);

			ResponseFunc::SetPublicHeader(m_header);

			ResponseFunc::SetContentType( m_header, fileExName);

			ResponseFunc::SetRangeAcceptedHeader(m_header);


			ResponseFunc::SendHeader(handle, m_header);
			ResponseFunc::LoopSendBuffer(handle, buf, start_range, length, is_Inverted_bits);
		}

	}

	static void SendStringContent(mt::mystring& content, std::shared_ptr<TcpSocket> handle, const mt::mystring& contentType) {
		
		mt::mystring m_header{};
		m_header.reserve(1024);

		m_header.append(MYTEXT("HTTP/1.1 200 OK\r\n"));

		ResponseFunc::SetContentLength(m_header, content.size());

		ResponseFunc::SetPublicHeader(m_header);

		m_header.append(MYTEXT("Content-Type: ")).append(contentType).append(MYTEXT("\r\n"));

		ResponseFunc::SendHeader(handle, m_header);

		handle->Write(content.data(), ::Integer_cast<size_t, DWORD>(content.size()));
	}

	static void SendJsonContent(mt::mystring& content, std::shared_ptr<TcpSocket> handle) {
		ResponseFunc::SendStringContent(content, handle, MYTEXT("application/json"));
	}

	static void SendHtmlContent(mt::mystring& content, std::shared_ptr<TcpSocket> handle) {
		ResponseFunc::SendStringContent(content, handle, MYTEXT("text/html; charset=utf-8"));
	}

	static void Send404(std::shared_ptr<TcpSocket> handle) {
		
		mt::mystring m_header{};
		m_header.reserve(1024);

		m_header.append(MYTEXT("HTTP/1.1 404 Not Found\r\n"));

		ResponseFunc::SetContentLength(m_header, 0);

		ResponseFunc::SetPublicHeader(m_header);

		ResponseFunc::SendHeader(handle, m_header);
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
	
			throw Win32SysteamException{ L"EnumFileFolder",error };
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
				throw Win32SysteamException{L"IsFileOrFolder", e };
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
	std::string m_file;

	std::string m_folder;


	void Add(bool isFolder, std::string& s, const std::string& path, const std::string& name) {
		
		s.append("<li><a href=\"");
		
		if(!UrlEncode::url_encode(s, path)){
			throw ArgumentException("url_encode error");
		}
		
		if (isFolder) {

			s.append("/\">");

		}
		else {

			s.append("\">");

		}

		s.append(name);

		s.append("</a></li>");
	}
	

public:

	Html():m_file(), m_folder(){

	}

	void Add(bool isFolder,  const std::string& path, const std::string& name){
		if(isFolder){
			this->Add(isFolder, m_folder, path, name);
		}
		else{
			this->Add(isFolder, m_file, path, name);
		}
	}

	std::string GetHtml() {

	
		std::string ret{};
		
		ret.append("<!DOCTYPE html><html lang=\"zh-cn\" xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta charset=\"utf-8\" /><title>文件和文件</title></head><body><div><div><ul>");
		
		ret.append(m_folder);
		
		ret.append("</ul></div><div><ul>");
		
		ret.append(m_file);
		
		ret.append("</ul></div></div></body></html>");

		return ret;
	}

};







struct RequestResponseAPI::RequestResponseData{

    HttpReqest* req;

    std::shared_ptr<TcpSocket> connect;

    RequestResponseData(HttpReqest* req, std::shared_ptr<TcpSocket> connect):req(req), connect(connect){

    }
};



struct RunServer::RunServerData{

    std::vector<mt::RoutIsFunc> is_func_vs;

    std::vector<mt::RoutFunc> func_vs;
};


void Response(RunServer::RunServerData* sd, std::shared_ptr<TcpSocket> handle, HttpReqest* request){

    RequestResponseAPI rr{std::make_unique<RequestResponseAPI::RequestResponseData>(request, handle)};

    

    auto& is_func_vs = sd->is_func_vs;

    auto& func_vs = sd->func_vs;



    for (decltype(is_func_vs.size()) n = 0; n < is_func_vs.size(); n++) {
        auto& is_func = is_func_vs[n];

        if(is_func(rr)){

            auto& func = func_vs[n];

            func(rr);

            return;
        }
    }

    ResponseFunc::Send404(handle);
}

void RequestLoop(RunServer::RunServerData* sd, std::shared_ptr<TcpSocket> handle){

	
	try {
		
		while (true)
		{

			auto request = HttpReqest::Read(handle);
			
			Response(sd, handle, request.get());
		}
	}
    catch (const ArgumentException& e) {

        auto s = UTF8::GetWideCharFromUTF8(e.what());
		MyWin32Out::Print(L"request loop ArgumentException:", s);

	}
	catch (const Win32SysteamException& e) {
		MyWin32Out::Print(L"request loop Win32SysteamException", e.what()); 
	}
	catch (const HttpReqest::FormatException& e) {
        auto s = UTF8::GetWideCharFromUTF8(e.what());
		MyWin32Out::Print(L"request loop request format error:", s);
	}
	catch (const SystemException& e) {
        auto s = UTF8::GetWideCharFromUTF8(e.what());
		MyWin32Out::Print(L"request loop SystemException :", s);

    }
    catch (const std::exception& e) {
        auto s = UTF8::GetWideCharFromUTF8(e.what());
		MyWin32Out::Print(L"request loop std::exception :", s);
	}
    catch(...){
       
        MyWin32Out::Exit(L"request loop throw other error :");
    }
}



RequestResponseAPI::RequestResponseAPI(std::unique_ptr<RequestResponseData> data) : pImpl(std::move(data)){}
RequestResponseAPI::~RequestResponseAPI()=default;
const mt::mystring& RequestResponseAPI::GetPath(){
    return pImpl->req->GetPath();
}

mt::Method RequestResponseAPI::GetMethod(){
	return pImpl->req->GetMethod();
}
mt::mystring RequestResponseAPI::GetQueryValue(const mt::mystring& key){
    return  pImpl->req->GetQueryValue(key);
}

void RequestResponseAPI::SendFile(const std::wstring& filePath, bool is_Inverted_bits){
    ResponseFunc::SendFile(filePath, pImpl->connect, *pImpl->req, is_Inverted_bits);
}
void RequestResponseAPI::SendBuffer(const std::wstring& fileExName,std::shared_ptr<std::vector< mt::byte >> buf, bool is_Inverted_bits){
    ResponseFunc::SendBuffer(fileExName, buf, pImpl->connect, *pImpl->req, is_Inverted_bits);
}


void RequestResponseAPI::SendStringContent(mt::mystring& content, const mt::mystring& contentType){
    ResponseFunc::SendStringContent(content,pImpl->connect, contentType);
}

void RequestResponseAPI::SendJsonContent(mt::mystring& content){
    ResponseFunc::SendJsonContent(content,pImpl->connect);
}

void RequestResponseAPI::SendHtmlContent(mt::mystring& content){
    ResponseFunc::SendHtmlContent(content,pImpl->connect);
}


void RequestResponseAPI::Send404(){
    ResponseFunc::Send404(pImpl->connect);
}


void RequestResponseAPI::SendFolderHtmlPage(const std::wstring& path){

    auto path2 = path;
    if (path2.ends_with(L'/')) {
        path2 += L'*';
    }
    else {
        path2 += L"/*";
    }

    EnumFileFolder eff{path2};
    EnumFileFolder::Data data{};
    Html html{};
    while (eff.Get(data))
    {
        std::string name= UTF8::GetUTF8FromWideChar(data.Path());

        html.Add(data.IsFolder(), name, name);
    }
    
    auto htmlStr = html.GetHtml();

    ResponseFunc::SendHtmlContent(htmlStr, pImpl->connect);

}

int RequestResponseAPI::IsFileOrFolder(const std::wstring& path){
    auto d = File::IsFileOrFolder(path);


    return d.IsFile()? 1 : d.IsFolder()? 2:0;
}

void RequestResponseAPI::ForeachFile(const std::wstring& path, std::function<void(const std::wstring& path, size_t size, bool isFolder)> func){
    auto path2 = path;
    if (path2.ends_with(L'/')) {
        path2 += L'*';
    }
    else {
        path2 += L"/*";
    }

    EnumFileFolder eff{path2};
    EnumFileFolder::Data data{};
  
    while (eff.Get(data))
    {
        func(data.Path(), data.Size(), data.IsFolder());
    }
}














RunServer::RunServer() : pImpl(std::make_unique<RunServerData>()) {}
RunServer::~RunServer() = default;  

void RunServer::Routing(mt::RoutIsFunc is_func,  mt::RoutFunc func){

    pImpl->func_vs.push_back(func);

    pImpl->is_func_vs.push_back(is_func);

}


void RunServer::Run(uint16_t port){

    Info::Initialization();

    auto f = new Fiber{};

    f->Start([](uint16_t port, RunServer::RunServerData* sd){

        TcpSocketListen lis{};
        lis.Bind(IPEndPoint{"0.0.0.0", port});
        lis.Listen(6);

        while (true)
        {
            auto connect = lis.Accept();

            Fiber::GetThis().Create(RequestLoop, sd, connect);
        }
        


    }, port,this->pImpl.get());

}



// 定义读取回调的类型
// 参数: 目标缓冲区(buffer), 要读取的字节数(size), 当前绝对偏移量(offset)
// 返回: 实际成功读取的字节数
using ReadCallback = std::function<std::streamsize(char* buffer, std::streamsize size, uint64_t offset)>;

class CustomInputStreambuf : public std::streambuf {
private:
    ReadCallback read_func;
    std::streamsize current_offset;
    const std::streamsize total_size;

public:
    CustomInputStreambuf(std::streamsize size, ReadCallback callback)
        : read_func(std::move(callback)), current_offset(0), total_size(size) {
        // 关闭标准 streambuf 的内置缓存
        setg(nullptr, nullptr, nullptr);
    }
	~CustomInputStreambuf()=default;
protected:
    // 核心：重写批量读取，直接绕过内置缓存，将数据塞入 s
    std::streamsize xsgetn(char_type* s, std::streamsize n) override {
        if (current_offset >= total_size) return 0; // 已到末尾

        // 确保不会读取超过总大小
        auto bytes_to_read = std::min(n, total_size - current_offset);
        
        // 调用用户的回调函数
        auto bytes_read = read_func(s, bytes_to_read, ::Integer_cast<std::streamsize, uint64_t>(current_offset));
        
        current_offset += bytes_read; // 更新偏移量
        return bytes_read;
    }

    // 处理单字节读取 (标准库备用机制)
    int_type underflow() override {
        if (current_offset >= total_size) {
            return traits_type::eof();
        }
        char c;
        if (xsgetn(&c, 1) == 1) {
            // 注意：因为没有使用缓存，我们需要马上把指针退回来，以便下次读取
            current_offset--; 
            return traits_type::to_int_type(c);
        }
        return traits_type::eof();
    }
    
    int_type uflow() override {
        if (current_offset >= total_size) {
            return traits_type::eof();
        }
        char c;
        if (xsgetn(&c, 1) == 1) {
            return traits_type::to_int_type(c); // uflow 自动推进偏移，不需要退回
        }
        return traits_type::eof();
    }

    // 核心：处理 7z 引擎的 Seek 跳转请求
    pos_type seekoff(off_type off, std::ios_base::seekdir dir,[[maybe_unused]]  std::ios_base::openmode which) override {
        if (dir == std::ios_base::beg) {
            current_offset = off;
        } else if (dir == std::ios_base::cur) {
            current_offset += off;
        } else if (dir == std::ios_base::end) {
            current_offset = total_size + off;
        }
        // 约束边界
        if (current_offset < 0) current_offset = 0;
        if (current_offset > total_size) current_offset = total_size;
        
        return current_offset;
    }

    pos_type seekpos(pos_type pos, [[maybe_unused]] std::ios_base::openmode which) override {
        current_offset = pos;
        return current_offset;
    }
};




CustomInputStream::CustomInputStream(const std::wstring& path) :std::istream(), buf(nullptr) {

	auto file = std::make_shared<CreateReadOnlyFile>(path);

	auto func = [file](char* buf, std::streamsize size, uint64_t offset) {
		

		auto n = file->Read(buf, ::Integer_cast<std::streamsize, DWORD>(size), offset);
	
		return ::Integer_cast<ULONG, std::streamsize>(n);

	};

	auto size = ::Integer_cast<LONGLONG, std::streamsize>(file->GetSize());

	auto p = std::make_unique<CustomInputStreambuf>(size, func);

	this->buf.swap(p);

	this->set_rdbuf(this->buf.get());
}


CustomInputStream::~CustomInputStream()=default;



class SequenceRun::SequenceRunClass{
private:
	std::queue<HANDLE> m_fiberQ;
	volatile uint32_t m_count;


	class P{
		SequenceRun::SequenceRunClass& m_v;
		public:

			P(SequenceRun::SequenceRunClass& v):m_v(v){
				m_v.m_count+=1;
				if(m_v.m_count > 1){
					auto f = Fiber::MyGetCurrentFiber();

					m_v.m_fiberQ.push(f);

					Fiber::GetThis().SwitchMain();
				}
			}

			~P(){
				m_v.m_count-=1;
				if(m_v.m_count > 0){

					auto f = m_v.m_fiberQ.front();

					m_v.m_fiberQ.pop();

					Fiber::GetThis().PostMain(f);
				}

			}
	};

public:

	SequenceRunClass(): m_fiberQ(), m_count(0){

	}


	void Run(std::function<void()>& func){

		P p{*this};
		
		func();

		
	}
};

SequenceRun::SequenceRun():pImpl(std::make_shared<SequenceRunClass>()){

}

SequenceRun::~SequenceRun()=default;

void SequenceRun::Run(std::function<void()>& func){

	this->pImpl->Run(func);
}