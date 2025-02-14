
#include "include/leikaifeng.h"
#include "myio.h"
#include <algorithm>
#include <filesystem>
#include <minwindef.h>
#include <ranges>
#include <string>


void Response(std::shared_ptr<TcpSocket> handle, std::unique_ptr<HttpReqest>& request, std::wstring& folderPath){
	
	auto path = UTF8::GetWideCharFromUTF8(request->GetPath());
	path =  folderPath + path;
	
	auto isff = File::IsFileOrFolder(path);

	if (isff.IsFile()) {

		HttpResponseFileContent response{path };
		
		response.SetRangeFromRequest(*request);

		response.Send(handle);


	}
	else if (isff.IsFolder()) {
		
		if (path.ends_with(L'/')) {
			path += L'*';
		}
		else {
			path += L"/*";
		}
	
		EnumFileFolder eff{path};
		EnumFileFolder::Data data{};
		Html html{};
		while (eff.Get(data))
		{
			std::string name= UTF8::GetUTF8ToString(data.Path());

			html.Add(data.IsFolder(), name, name);
		}
		


		HttpResponseStrContent response{ 200, std::move(html.GetHtml())};

		response.Send(handle);
	}
	else {
		Print("path error   ", ::UTF8::GetMultiByte(path));
		
		HttpResponse404 response{};


		response.Send(handle);
	}
}


void RequestLoop(std::shared_ptr<TcpSocket> handle, std::wstring folderPath){

	
	try {
		int n = 0;

		while (true)
		{

			auto request = HttpReqest::Read(handle);
			
			Response(handle, request, folderPath);
			n++;

			Print(n, "re use link");
		}
	}
	catch (Win32SysteamException& e) {
		Print(e.what()); 
	}
	catch (HttpReqest::FormatException& e) {
		Print("request format error:", e.what());
	}
	catch (::SystemException& e) {
		Print("SystemException :", e.what());
	}
}

void NewAcceptAction(SOCKET s, std::wstring path){
	auto handle = std::make_shared<TcpSocket>(s);

	Fiber::GetThis().Create(RequestLoop, handle, path);

}




std::wstring GetExePath(){

    wchar_t szFileName[MAX_PATH];

    auto res = GetModuleFileNameW(NULL, szFileName, MAX_PATH);
    auto error = GetLastError();

    if(res != 0 && error != ERROR_INSUFFICIENT_BUFFER){
        return  std::wstring{szFileName, res};
    }
    else{
        Exit("GetModuleFileNameW error", (int)error);   
        return std::wstring{};
    }
    
}


std::wstring GetExeFolder(){
    auto exePath = GetExePath();

    std::filesystem::path p{exePath};

    return  p.parent_path();


}


struct InputArgs{
	USHORT port;

	std::wstring path;
};


InputArgs GetInputArgs(int argc, char *argv[]){
	std::vector<std::wstring> vs{};
	
	std::ranges::for_each(std::views::counted(argv, argc), [&vs](const auto& item)->void{

		vs.push_back(UTF8::GetWideCharFromMultiByte(item));
	});

	auto const windows = vs | std::views::slide(2);

	InputArgs value{};

	value.port=80;

	value.path= GetExeFolder();


	std::ranges::for_each(windows, [&value](const auto& item)->void{
		USHORT port;
		if(item[0] == L"-p" && Number::Parse(UTF8::GetUTF8ToString(item[1]), port)){
			
			value.port= port;
		}

		if(item[0] == L"-d"){
			value.path= item[1];
		}
	});

	return value;
}






int main(int argc, char *argv[]) {
	Print("args", "-d is folder", "-p is port");
	
	auto inputArgs = GetInputArgs(argc, argv);

	auto wpath = inputArgs.path;
	auto port = inputArgs.port;
	
	Print("path", UTF8::GetMultiByte(wpath), "port", port);


	std::replace(wpath.begin(), wpath.end(), L'\\', L'/');
	Info::Initialization();

	//之所以使用同步ACCEPT是因为当前Fiber模型
	//每一个线程独自使用一个io完成端口
	//侦听socket跟其中一个线程的io完成端口绑定后无法将传入的socket链接派发给其他线程
	TcpSocketListenSync lis{};
	
	lis.Bind(IPEndPoint("0.0.0.0", port));
	
	lis.Listen(16);

	std::vector<Fiber*> f_v{};

	std::vector<std::thread> t_v{};

	size_t count = 3;

	for (size_t i = 0; i < count; i++)
	{
		auto f = new Fiber{};

		std::thread t{[](auto fiber){
			fiber->Start([](){});
			
		}, f};

		f_v.push_back(f);

		t_v.push_back(std::move(t));
	}
	

	size_t n=0;
	while (true)
	{
		n++;

		auto s = lis.Accept();

		auto index = n% count;

		f_v[index]->Create_ThreadSafe(NewAcceptAction, s, wpath);
	}
	
	
}
