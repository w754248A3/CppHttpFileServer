
#include <bitarchivereader.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <cerrno>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include "include/leikaifeng.h"
#include "include/myio.h"
#include "myio.h"


class MyZipReader2 : Delete_Base{

private:

    std::string extract_target_content(const std::string_view& input)
    {
        constexpr std::string_view keyword = "password_";

        // Search from the end of the string for the keyword
        size_t pos = input.rfind(keyword);
        if (pos == std::string::npos)
        {
            return ""; // Keyword not found
        }
        
        size_t start = pos + keyword.length();

        std::string_view remaining = std::string_view(input).substr(start);

        size_t end = remaining.rfind('.');
        if (end != std::wstring_view::npos){
            remaining = remaining.substr(0, end);
        }
        



        return std::string(remaining);
    }

    class MyNeedData{
        public:
            uint32_t index;
            size_t size;
            std::string path;
            std::string exname;
            std::shared_ptr<std::vector<byte>> fileData;
            size_t count;
        MyNeedData(uint32_t index,
            size_t size,
            std::string path,
            std::string exname):
            index(index), size(size),
            path(std::move(path)),
            exname(std::move(exname)),
            fileData(),
            count(0){

            }
    };

    constexpr static size_t MAXINDEX =  std::numeric_limits<size_t>::max(); 

    bit7z::Bit7zLibrary m_lib;
    std::wstring m_path;
    std::unique_ptr<bit7z::BitArchiveReader> m_arc;

    std::unordered_map<uint32_t, MyNeedData> m_data; 
    
public:
    //初始化的顺序很重要
    MyZipReader2(const std::wstring& dllPath):
     m_lib(bit7z::to_tstring(dllPath)),
     m_path(),
     m_arc(),
     m_data()
   
    {
       
    }

    const bit7z::BitInFormat &detectRAR(const std::string &in_file, const std::string &password)
    {

        try
        {
            bit7z::BitArchiveReader info(m_lib, in_file, bit7z::BitFormat::Rar5, password);
          
            return bit7z::BitFormat::Rar5;
        }
        catch (const bit7z::BitException &)
        {
           
            bit7z::BitArchiveReader info(m_lib, in_file, bit7z::BitFormat::Rar, password);
            return bit7z::BitFormat::Rar;
        }
    }

    void OpenFile(const std::wstring& path, const bit7z::BitInFormat& format){
        if(path == m_path){
            return;
        }

        m_path= path;
        

        
        auto fv = &format;
        auto u8path =  bit7z::to_tstring(path);

        const auto password = extract_target_content(u8path);

        Print("password:", password);
        if((*fv) == bit7z::BitFormat::Rar){
             fv = &detectRAR(u8path, password);
        }


        

        m_arc = std::make_unique<bit7z::BitArchiveReader>(m_lib, 
        u8path, 
        *fv,
        password);
        
        m_data.clear();
      
        try{
            auto arc_items = m_arc->items();
            for (auto &item : arc_items)
            {
                if(item.isDir()){

                }
                else{
                    
                    auto index = item.index();

                    auto path = item.path();

                    auto size = item.size();

                    auto exname = item.extension();

                    m_data.emplace(index, MyNeedData{index, size, 
                        path,
                        exname,
                    });
                }

            }
        }
        catch (const bit7z::BitException &ex)
        {

            Exit(ex.what());
        }

        

    }

    // 估算可分配的最大内存（物理内存和虚拟地址空间的最小值 * 保守估算%）
    size_t GetMaxAllocatablePhysicalMemory()
    {
        MEMORYSTATUSEX memInfo = {};
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (!GlobalMemoryStatusEx(&memInfo)) {
            return 0;
        }

        ULONGLONG availPhys = memInfo.ullAvailPhys;
        ULONGLONG availVirtual = memInfo.ullAvailVirtual;
        ULONGLONG limit = (availPhys < availVirtual) ? availPhys : availVirtual;

        return static_cast<size_t>(static_cast<double>( limit) * 0.7); // 保守估算
    }

    void TryCanNeedRemove(MyNeedData& data){

        auto v = GetMaxAllocatablePhysicalMemory();

        if(data.size< v){
            return;
        }
        

        for (auto& item : m_data) {
            

            item.second.fileData = nullptr;

            item.second.count=0;
            
        }

    }

    bool GetBytes(uint32_t index, std::shared_ptr<std::vector<bit7z::byte_t>>& fileData, std::string& exname){
        
        auto v = m_data.find(index);

        if(v == m_data.end()){

            return false;
        }

        MyNeedData& data = v->second;

        exname = data.exname;
        
        if(data.count != 0){
            fileData =data.fileData;
            data.count+=1;
            return true;
        }

        TryCanNeedRemove(data);


        data.fileData = std::make_shared<std::vector<bit7z::byte_t>>();
        try{
            
            m_arc->extractTo(*data.fileData, ::Integer_cast<size_t, uint32_t>(index));
            
            fileData= data.fileData;
            data.count=1;
            return true;
        }
        catch (const bit7z::BitException &ex)
        {

            Print("extractTo error", ex.what());
        }

       
        return false;
    }

    void GetFileNameAndIndex(std::function<void(uint32_t, const std::string&)> func){

        for (const auto& item: m_data)
        {
           func(item.second.index, item.second.path);
        }
        
    }

};


bool GetBitInFormat(const std::wstring& filePath, bit7z::BitInFormat const * * v){
     std::filesystem::path path{filePath};

    if(!path.has_extension()){
        return false;
    }
    auto ex = path.extension();
    
    if(ex ==L".zip"){
        *v = &bit7z::BitFormat::Zip;
        return true;
    }
    else if(ex == L".rar"){
        *v = &bit7z::BitFormat::Rar;
        return true;
    }
    else if(ex == L".7z"){
        *v = &bit7z::BitFormat::SevenZip;
        return true;
    }
    else{

        return false;
    }
    
}

void Response2(std::shared_ptr<TcpSocket> handle, std::unique_ptr<HttpReqest>& request, std::shared_ptr<MyZipReader2> reader, std::wstring& filePath){
	
    const bit7z::BitInFormat* v;

    if(!GetBitInFormat(filePath, &v)){
       
        ResponseFunc::Send(filePath, handle, *request);


        return;
    }


    reader->OpenFile(filePath, *v);

    auto isjsonstr = request->GetQueryValue(MYTEXT("json"));

    if(isjsonstr == MYTEXT("1")){

        Print("is file json");
        boost::json::array vs{};

        reader->GetFileNameAndIndex([&vs](uint32_t index, const std::string& name){

            
            boost::json::object kv{};
            kv.emplace("index",index);
            
            kv.emplace("path", name);
            
            vs.emplace_back(kv);

        });
        auto cont = boost::json::serialize(vs);
      
        ResponseFunc::SendJsonContent( cont, handle);

        return;
    }


    auto indexstring = request->GetQueryValue(MYTEXT("Index"));


    if(indexstring == MYTEXT("")){
         Html html {};
        Print("is file html");
        reader->GetFileNameAndIndex([&html](uint32_t index, const std::string& name){

            mt::mystring path {"?Index="};
            Number::ToString(path, index);
            
            
            html.Add(false, path, name);

        });

        auto htmlStr = html.GetHtml();
        ResponseFunc::SendHtmlContent( htmlStr, handle);

        

        return;
    }

    size_t index;
  
    if(!Number::Parse(indexstring, index)){

        ResponseFunc::Send404(handle);

        return;
    }


    Print(index);
    std::string exname{};
    std::shared_ptr<std::vector<bit7z::byte_t>> buf{};
    if(!reader->GetBytes(static_cast<uint32_t>(index), buf, exname)){
        ResponseFunc::Send404(handle);

        return;
    }
    exname.insert(0, ".");

    ResponseFunc::SendBuffer(UTF8::GetWideCharFromUTF8(exname), buf, handle, *request);
}



void Response(std::shared_ptr<TcpSocket> handle, std::unique_ptr<HttpReqest>& request, std::shared_ptr<MyZipReader2> reader, std::wstring& folderPath, std::wstring& appPath){
	
	auto path = UTF8::GetWideCharFromUTF8(request->GetPath());

    Print("path:", UTF8::GetMultiByte(path));
    if(path.starts_with(L"/app")){
        path = appPath +path.substr(4);

        if(File::IsFileOrFolder(path).IsFile()){
           
            ResponseFunc::Send (path, handle, *request);


        }
        else{

            ResponseFunc::Send404(handle);

        }



        return;
    }
    

	path =  folderPath + path;
	
	auto isff = File::IsFileOrFolder(path);

	if (isff.IsFile()) {
        Print("is file");
        Response2(handle, request, reader, path);
		

	}
	else if (isff.IsFolder()) {
		Print("IsFolder");
		if (path.ends_with(L'/')) {
			path += L'*';
		}
		else {
			path += L"/*";
		}
	
		EnumFileFolder eff{path};
		EnumFileFolder::Data data{};



        auto isjsonstr = request->GetQueryValue(MYTEXT("json"));

        if(isjsonstr == MYTEXT("1")){
            Print("IsFolder json");
            boost::json::array vs{};


            while (eff.Get(data))
            {
                std::wstring name{data.Path()};
                auto u8 = UTF8::GetUTF8ToString(name);
               boost::json::object kv{};

               kv.emplace("isfolder",data.IsFolder());
                
                kv.emplace("name", u8);
                
                vs.emplace_back(kv);
            }

            {
                auto cont = boost::json::serialize(vs);
             
                ResponseFunc::SendJsonContent ( cont, handle);


            }
            

            

            return;
        }


        Print("IsFolder html");
		Html html{};
		while (eff.Get(data))
		{
			std::string name= UTF8::GetUTF8ToString(data.Path());



			html.Add(data.IsFolder(),  name, name);
		}
		


        auto htmlStr =  html.GetHtml();
        ResponseFunc::SendHtmlContent(htmlStr, handle);


	}
	else {
		Print("path error   ", ::UTF8::GetMultiByte(path));
		
		ResponseFunc::Send404(handle);
	}
}



void RequestLoop(std::shared_ptr<TcpSocket> handle, std::shared_ptr<MyZipReader2> reader, std::wstring path, std::wstring apppath){

	
	try {
		int n = 0;

		while (true)
		{

			auto request = HttpReqest::Read(handle);
			
			Response(handle, request, reader, path, apppath);
			n++;

			Print(n, "re use link");
		}
	}
    catch (const ArgumentException& e) {
		Print("request loop ArgumentException:", e.what());

	}
	catch (const Win32SysteamException& e) {
		Print("request loop Win32SysteamException", e.what()); 
	}
	catch (const HttpReqest::FormatException& e) {
		Print("request loop request format error:", e.what());
	}
	catch (const SystemException& e) {
		Print("request loop SystemException :", e.what());

    }
    catch (const std::exception& e) {
		Print("request loop std::exception :", e.what());
	}
    catch(...){
       
        Exit("request loop throw other error :");
    }
}


int main(int argc, char *argv[]) {
	if(argc != 3){
		Exit("argce != 3,  args  app path, file path");

		return 0;
	}

	
    std::string apppath{argv[1]};

    auto wapppath = ::UTF8::GetWideCharFromMultiByte(apppath);
    std::replace(wapppath.begin(), wapppath.end(), L'\\', L'/');

    std::string path{argv[2]};

	auto wpath = ::UTF8::GetWideCharFromMultiByte(path);
    std::replace(wpath.begin(), wpath.end(), L'\\', L'/');

	std::wstring dllpath{L"7z.dll"};

    auto reader = std::make_shared<MyZipReader2>(dllpath);


	Info::Initialization();

    auto f = new Fiber{};

    f->Start([](std::shared_ptr<MyZipReader2> p, std::wstring wpath, std::wstring wapppath){

        TcpSocketListen lis{};
        lis.Bind(IPEndPoint{"0.0.0.0", 80});
        lis.Listen(6);

        while (true)
        {
            auto connect = lis.Accept();

            Fiber::GetThis().Create(RequestLoop, connect, p, wpath, wapppath);
        }
        


    }, reader, wpath, wapppath);
}
