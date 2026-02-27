#include "myserverapi.h"
#include "myio.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>



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

    auto&  path = request->GetPath();

    RequestResponseAPI rr{std::make_unique<RequestResponseAPI::RequestResponseData>(request, handle)};

    

    auto& is_func_vs = sd->is_func_vs;

    auto& func_vs = sd->func_vs;



    for (decltype(is_func_vs.size()) n = 0; n < is_func_vs.size(); n++) {
        auto& is_func = is_func_vs[n];

        if(is_func(path)){

            auto& func = func_vs[n];

            func(rr);

            return;
        }
    }

    ResponseFunc::Send404(handle);
}

void RequestLoop(RunServer::RunServerData* sd, std::shared_ptr<TcpSocket> handle){

	
	try {
		int n = 0;

		while (true)
		{

			auto request = HttpReqest::Read(handle);
			
			Response(sd, handle, request.get());
			n++;

			MyWin32Out::Print(n, L"re use link");
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

    EnumFileFolder eff{path};
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

