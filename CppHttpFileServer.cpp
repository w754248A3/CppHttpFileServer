
#include "include/leikaifeng.h"
#include "include/myserverapi.h"
#include <algorithm>
#include <filesystem>
#include <minwindef.h>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <fcntl.h>  // _O_U16TEXT
#include <io.h>     // _setmode


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


		if(item[0] == L"-p"){
			
			auto u8 = UTF8::GetUTF8FromWideChar(item[1]);

			auto res = std::from_chars(u8.data(), u8.data() + u8.size(), port);

			if(res.ec == std::errc{}){
				value.port= port;
			}
		

			
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


	RunServer rs{};


	rs.Routing([](const mt::mystring& path){
		return true;
	},
	[&folderPath= wpath](RequestResponseAPI& p){

		auto& path = p.GetPath();
		auto req_wpath = UTF8::GetWideCharFromUTF8(path);

		auto all_wpath = folderPath  +req_wpath;
		MyWin32Out::Print(all_wpath);
		auto is_folder_file = p.IsFileOrFolder(all_wpath);
	
		if(is_folder_file==1){
			p.SendFile(all_wpath,false);
		}
		else if(is_folder_file==2){
			p.SendFolderHtmlPage(all_wpath);
		}
		else{
			p.Send404();
		}
	});

	rs.Run(port);

	return 0;
}
