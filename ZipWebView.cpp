
#include <bitarchivereader.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <minwindef.h>
#include <string>
#include <string_view>
#include <utility>
#include "include/mypublicapi.h"
#include "include/myserverapi.h"
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

            MyWin32Out::Exit(UTF8::GetWideCharFromUTF8(ex.what()));
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

void StaticFileRouting(RequestResponseAPI& p, const std::wstring& folderPath){
    
    auto& path = p.GetPath();
    auto req_wpath = UTF8::GetWideCharFromUTF8(path);

    auto all_wpath = folderPath  +req_wpath.substr(4);
    MyWin32Out::Print(L"/app path  ",all_wpath);
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


void FileRouting2(RequestResponseAPI& p, const std::wstring& filePath, std::shared_ptr<MyZipReader2> reader){
    

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


    reader->OpenFile(filePath, *v);

    auto isjsonstr = p.GetQueryValue(MYTEXT("json"));

    if(isjsonstr == MYTEXT("1")){

        MyWin32Out::Print(L"is file json");
        boost::json::array vs{};

        reader->GetFileNameAndIndex([&vs](uint32_t index, const std::string& name){

            
            boost::json::object kv{};
            kv.emplace("index",index);
            
            kv.emplace("path", name);
            
            vs.emplace_back(kv);

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
    if(!reader->GetBytes(static_cast<uint32_t>(index), buf, exname)){
        p.Send404();

        return;
    }
    exname.insert(0, ".");

    p.SendBuffer(UTF8::GetWideCharFromUTF8(exname), buf, is_Inverted_bits);
}


void FileRouting(RequestResponseAPI& p, const std::wstring& folderPath, std::shared_ptr<MyZipReader2> reader){
    
    
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

            p.ForeachFile(all_wpath, [&vs](const std::wstring& name, size_t size, bool isfolder){
               
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

    auto reader = std::make_shared<MyZipReader2>(dllpath);

    RunServer rs{};

    //静态文件路由
	rs.Routing([](RequestResponseAPI& p){
		return p.GetPath().starts_with(MYTEXT("/app"));
	},
	[&folderPath=wapppath](RequestResponseAPI& p){
        StaticFileRouting(p, folderPath);
	});



    rs.Routing([](RequestResponseAPI& p){
		return !p.GetPath().starts_with(MYTEXT("/app"));
	},
	[&folderPath=wpath, &reader](RequestResponseAPI& p){
        FileRouting(p, folderPath, reader);
	});

	rs.Run(80);

    return 0;
}
