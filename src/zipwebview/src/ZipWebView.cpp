
#include <bitarchivereader.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <istream>
#include <memory>
#include <minwindef.h>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include "mypublicapi.h"
#include "myserverapi.h"
#include <fcntl.h>  // _O_U16TEXT
#include <io.h>     // _setmode

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
    std::unique_ptr<CustomInputStream> m_stream;
    std::unique_ptr<bit7z::BitArchiveReader> m_arc;

    std::unordered_map<uint32_t, MyNeedData> m_data; 
    
public:
    //初始化的顺序很重要
    MyZipReader2(const std::wstring& dllPath):
     m_lib(bit7z::to_tstring(dllPath)),
     m_path(),
     m_stream(),
     m_arc(),
     m_data()
   
    {
       
    }

    const bit7z::BitInFormat &detectRAR(std::istream& inStream, const std::string &password)
    {

        try
        {
            bit7z::BitArchiveReader info(m_lib, inStream, bit7z::BitFormat::Rar5, password);
            inStream.clear();
            inStream.seekg(0, std::ios::beg);
            return bit7z::BitFormat::Rar5;
        }
        catch (const bit7z::BitException &)
        {
            inStream.clear();
            inStream.seekg(0, std::ios::beg);
            bit7z::BitArchiveReader info(m_lib, inStream, bit7z::BitFormat::Rar, password);
            inStream.clear();
            inStream.seekg(0, std::ios::beg);
            return bit7z::BitFormat::Rar;
        }
    }

    bool OpenFile(const std::wstring& path, const bit7z::BitInFormat& format){
        if(path == m_path){
            return true;
        }

        m_path= path;
        

        
        auto fv = &format;
        auto u8path =  bit7z::to_tstring(path);

        const auto password = extract_target_content(u8path);

        try{
            
            

            m_stream= std::make_unique<CustomInputStream>(path);

            if((*fv) == bit7z::BitFormat::Rar){
                fv = &detectRAR(*m_stream, password);
            }

            m_arc = std::make_unique<bit7z::BitArchiveReader>(m_lib, 
            *m_stream, 
            *fv,
            password);
            
            m_data.clear();
      
        }
        catch(const bit7z::BitException &ex){
            MyWin32Out::Print(L"open zip pack error");
            
            return false;
        }
        
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
            
            MyWin32Out::Print(L"get zip pack list error");
           
            return false;
        }

        return true;

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
        
        const size_t MIN_BUFFER_SIZE = 1024*1024*30;

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


        fileData = std::make_shared<std::vector<bit7z::byte_t>>();
        try{
            


            m_arc->extractTo(*fileData, ::Integer_cast<size_t, uint32_t>(index));
            
            if(fileData->size() >= MIN_BUFFER_SIZE){
                
                data.fileData=fileData;
                data.count=1;
                return true;
            }
            else{
                data.fileData=nullptr;
                data.count=0;
                return true;
            }

        }
        catch (const bit7z::BitException &ex)
        {
            
            auto s = UTF8::GetWideCharFromUTF8(ex.what());
            MyWin32Out::Print(L"extractTo error", s);
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


class MySequenceRunData{
private:

    std::shared_ptr<MyZipReader2> _reader;
  
    SequenceRun _run;

public:
    MySequenceRunData(std::wstring dllpath):_reader(), _run(){

        _reader= std::make_shared<MyZipReader2>(dllpath);
    }


    void Run(std::function<void(std::shared_ptr<MyZipReader2> reader)>&& func){
        
        std::function<void()> f = [reader= _reader, &func](){
            func(reader);
        };

        _run.Run(f);

        
    }

};

void StaticFileRouting(RequestResponseAPI& p, const std::wstring& folderPath){
    
    auto& path = p.GetPath();
    auto req_wpath = UTF8::GetWideCharFromUTF8(path);

    auto all_wpath = folderPath  +req_wpath.substr(4);
  
    auto is_folder_file = p.IsFileOrFolder(all_wpath);

    if(is_folder_file==1){
        p.SendFile(all_wpath,false);
    }
    else{
        p.Send404();
    }
}

bool ParseNumber(const mt::mystring& s, size_t& n){
    auto res = std::from_chars(s.data(), s.data() + s.size(), n);

    return  res.ec == std::errc{};
		
}


void FileRouting2(RequestResponseAPI& p, const std::wstring& filePath, std::shared_ptr<MySequenceRunData> msrd){
    

    bool is_Inverted_bits = false;

    {
        auto isjsonstr = p.GetQueryValue(MYTEXT("ib"));

        is_Inverted_bits = isjsonstr == MYTEXT("1");
    }

    const bit7z::BitInFormat* v;

    if(!GetBitInFormat(filePath, &v)){

        p.SendFile(filePath, is_Inverted_bits);

        return;
    }

    bool isopen = false;
    msrd->Run([ &isopen, &filePath, &v](std::shared_ptr<MyZipReader2> reader){
        isopen = reader->OpenFile(filePath, *v);
    });

    if(!isopen){

        p.Send404();
        return;
    }

    auto isjsonstr = p.GetQueryValue(MYTEXT("json"));

    if(isjsonstr == MYTEXT("1")){

        MyWin32Out::Print(L"is file json");
        boost::json::array vs{};

        msrd->Run([&vs](std::shared_ptr<MyZipReader2> reader){
            
            reader->GetFileNameAndIndex([&vs](uint32_t index, const std::string& name){

                
                boost::json::object kv{};
                kv.emplace("index",index);
                
                kv.emplace("path", name);
                
                vs.emplace_back(kv);

            });
        });

        auto cont = boost::json::serialize(vs);
      
        p.SendJsonContent(cont);

        return;
    }


    auto indexstring = p.GetQueryValue(MYTEXT("Index"));



    size_t index;
  
    if(!ParseNumber(indexstring, index)){

        p.Send404();

        return;
    }


    MyWin32Out::Print(index);
    std::string exname{};
    std::shared_ptr<std::vector<bit7z::byte_t>> buf{};

    bool b=false;
    msrd->Run([&b, index, &buf, &exname](std::shared_ptr<MyZipReader2> reader){
        b = reader->GetBytes(static_cast<uint32_t>(index), buf, exname);
    });



    if(!b){
        p.Send404();

        return;
    }
    exname.insert(0, ".");

    p.SendBuffer(UTF8::GetWideCharFromUTF8(exname), buf, is_Inverted_bits);
}


void FileRouting(RequestResponseAPI& p, const std::wstring& folderPath, std::shared_ptr<MySequenceRunData> reader){
    
    
    auto& path = p.GetPath();
    auto req_wpath = UTF8::GetWideCharFromUTF8(path);

    auto all_wpath = folderPath  +req_wpath;
    MyWin32Out::Print(L"/ path  ",all_wpath);
    auto is_folder_file = p.IsFileOrFolder(all_wpath);

    if(is_folder_file==1){
        FileRouting2(p, all_wpath, reader);
    }
    else if(is_folder_file == 2){
        
        auto isjsonstr = p.GetQueryValue(MYTEXT("json"));

        if(isjsonstr == MYTEXT("1")){
            MyWin32Out::Print(L"IsFolder json");
            boost::json::array vs{};

            p.ForeachFile(all_wpath, [&vs](const std::wstring& name,[[maybe_unused]] size_t size, bool isfolder){
               
                auto u8 = UTF8::GetUTF8FromWideChar(name);
                boost::json::object kv{};

                kv.emplace("isfolder",isfolder);
                
                kv.emplace("name", u8);
                
                vs.emplace_back(kv);
            });
            

            {
                auto cont = boost::json::serialize(vs);
            
                p.SendJsonContent(cont);

            }
        

        
        }
        else{
            p.Send404();
        }

    }
    else{
        p.Send404();
    }
}

auto& GetFileList(){

    static std::unordered_set<mt::mystring> set;

    return set;
}

void PostFileName(RequestResponseAPI& p, const std::wstring& folderPath){
    if(p.GetQueryValue(MYTEXT("set"))==MYTEXT("1")){

        auto path = folderPath+ UTF8::GetWideCharFromUTF8( p.GetPath());
        MyWin32Out::Print(L"post name:", path);
        if(p.IsFileOrFolder(path) ==0){
            MyWin32Out::Print(L"post name error:", path);
            p.Send404();
            return;
        }

        auto& set = GetFileList();

        set.insert(UTF8::GetUTF8FromWideChar(path));

        boost::json::object kv{};
        kv.emplace("resultCode",true);
        
        auto cont = boost::json::serialize(kv);

        p.SendJsonContent(cont);

    }
    else if(p.GetQueryValue(MYTEXT("get"))==MYTEXT("1")){
        boost::json::array vs{};
        auto& set = GetFileList();
        for (auto& p : set) {
            vs.emplace_back(p);
        }

        boost::json::object kv{};
        kv.emplace("resultCode",true);
        kv.emplace("list", vs);

        auto cont = boost::json::serialize(kv);

        p.SendJsonContent(cont);

    }
    else{
        p.Send404();
    }
   
}

int wmain(int argc, wchar_t* argv[]) {


    _setmode(_fileno(stdin), _O_U16TEXT);
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);


	if(argc != 3){
		MyWin32Out::Exit(L"argce != 3,  args  app path, file path");

		return 0;
	}

	
    std::wstring wapppath{argv[1]};
    std::replace(wapppath.begin(), wapppath.end(), L'\\', L'/');

    std::wstring wpath{argv[2]};

    std::replace(wpath.begin(), wpath.end(), L'\\', L'/');

	std::wstring dllpath{L"7z.dll"};

    auto reader = std::make_shared<MySequenceRunData>(dllpath);

    RunServer rs{};

    //静态文件路由
	rs.Routing([](RequestResponseAPI& p){
		return p.GetMethod() == mt::Method::GET && p.GetPath().starts_with(MYTEXT("/app"));
	},
	[&folderPath=wapppath](RequestResponseAPI& p){
        StaticFileRouting(p, folderPath);
	});



    rs.Routing([](RequestResponseAPI& p){
		return p.GetMethod() == mt::Method::GET && !p.GetPath().starts_with(MYTEXT("/app"));
	},
	[&folderPath=wpath, &reader](RequestResponseAPI& p){
        FileRouting(p, folderPath, reader);
	});

    rs.Routing([](RequestResponseAPI& p){
        return p.GetMethod() == mt::Method::POST;
    },
	[&folderPath=wpath](RequestResponseAPI& p){
        PostFileName(p, folderPath);
	});

    MyWin32Out::Print(L"new project");
	rs.Run(80);

    return 0;
}
