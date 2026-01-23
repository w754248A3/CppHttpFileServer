
#include "include/leikaifeng.h"
#include "myio.h"
#include <algorithm>
#include <filesystem>
#include <minwindef.h>
#include <ranges>
#include <string>
#include <utility>
#include <fcntl.h>  // _O_U16TEXT
#include <io.h>     // _setmode

void Response(std::shared_ptr<TcpSocket> handle, std::unique_ptr<HttpReqest>& request, std::wstring& folderPath){
	
	auto path = UTF8::GetWideCharFromUTF8(request->GetPath());
	path =  folderPath + path;
	
	auto isff = File::IsFileOrFolder(path);

	if (isff.IsFile()) {

		ResponseFunc::SendFile (path, handle, *request, false);


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
			std::string name= UTF8::GetUTF8FromWideChar(data.Path());

			html.Add(data.IsFolder(), name, name);
		}
		
		auto htmlStr = html.GetHtml();

		ResponseFunc::SendHtmlContent(htmlStr, handle);

	}
	else {
		MyWin32Out::Print(L"path error   ", path);
		
		ResponseFunc::Send404(handle);
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

			MyWin32Out::Print(n, L"re use link");
		}
	}
	catch (Win32SysteamException& e) {
		MyWin32Out::Print(e.what()); 
	}
	catch (HttpReqest::FormatException& e) {
		MyWin32Out::Print(L"request format error:", UTF8::GetWideCharFromUTF8(e.what()));
	}
	catch (::SystemException& e) {
		MyWin32Out::Print(L"SystemException :",UTF8::GetWideCharFromUTF8(e.what()));
	}
}



std::wstring GetExePath(){

    wchar_t szFileName[MAX_PATH];

    auto res = GetModuleFileNameW(NULL, szFileName, MAX_PATH);
    auto error = GetLastError();

    if(res != 0 && error != ERROR_INSUFFICIENT_BUFFER){
        return  std::wstring{szFileName, res};
    }
    else{
        MyWin32Out::Exit(L"GetModuleFileNameW error", (int)error);   
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


InputArgs GetInputArgs(int argc, wchar_t* argv[]){
	std::vector<std::wstring> vs{};
	
	std::ranges::for_each(std::views::counted(argv, argc), [&vs](const auto& item)->void{

		vs.push_back(item);
	});

	auto const windows = vs | std::views::slide(2);

	InputArgs value{};

	value.port=80;

	value.path= GetExeFolder();


	std::ranges::for_each(windows, [&value](const auto& item)->void{
		USHORT port;
		if(item[0] == L"-p" && Number::Parse(UTF8::GetUTF8FromWideChar(item[1]), port)){
			
			value.port= port;
		}

		if(item[0] == L"-d"){
			value.path= item[1];
		}
	});

	return value;
}






int wmain(int argc, wchar_t* argv[]) {
	
	 _setmode(_fileno(stdin), _O_U16TEXT);
     _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);


	MyWin32Out::Print(L"args", L"-d is folder", L"-p is port");
	
	auto inputArgs = GetInputArgs(argc, argv);

	auto wpath = inputArgs.path;
	auto port = inputArgs.port;
	
	MyWin32Out::Print(L"path",wpath, L"port", port);


	std::replace(wpath.begin(), wpath.end(), L'\\', L'/');
	Info::Initialization();




	Fiber fiber{};

	fiber.Start([](USHORT port, std::wstring path){
		TcpSocketListen lis{};
		lis.Bind(IPEndPoint("0.0.0.0", port));
	
		lis.Listen(16);
		
		while (true) {
		
			auto con = lis.Accept();

			Fiber::GetThis().Create(RequestLoop, con, path);
		}

	}, port, wpath);

	return 0;
}
