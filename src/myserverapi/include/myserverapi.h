#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <streambuf>
#ifndef _MYSERVERAPI
#define _MYSERVERAPI


#define MYTEXT(args) args

class RequestResponseAPI;
namespace mt {


using mystring = std::string;

using mystring_view= std::string_view;

using mychar = char;

using byte = unsigned char;

using RoutIsFunc =std::function<bool(RequestResponseAPI&)>;

using RoutFunc = std::function<void(RequestResponseAPI&)>;

enum Method {
    GET,
    HEAD,
    POST,
};
}


class RequestResponseAPI{


public:

    const mt::mystring&  GetPath();

    mt::mystring GetQueryValue(const mt::mystring& key);

    mt::Method GetMethod();

    void SendFile(const std::wstring& filePath, bool is_Inverted_bits);

    void SendBuffer(const std::wstring& fileExName,std::shared_ptr<std::vector< mt::byte >> buf, bool is_Inverted_bits);
   
    void SendStringContent(mt::mystring& content, const mt::mystring& contentType);
    void SendJsonContent(mt::mystring& content);
    void SendHtmlContent(mt::mystring& content);
    void Send404();

    void SendFolderHtmlPage(const std::wstring& path);


    int IsFileOrFolder(const std::wstring& path);

    void ForeachFile(const std::wstring& path, std::function<void(const std::wstring& path, size_t size, bool isFolder)> func);
    
    


    struct RequestResponseData; 
    RequestResponseAPI(std::unique_ptr<RequestResponseData> data);  
    ~RequestResponseAPI();
    
private:
    
                    
    std::unique_ptr<RequestResponseData> pImpl;
};




class RunServer{

public:
    
void Routing(mt::RoutIsFunc is_func,  mt::RoutFunc func);

void Run(uint16_t port);

RunServer();
~RunServer();

struct RunServerData;  
private:
                       
    std::unique_ptr<RunServerData> pImpl;
};





class CustomInputStreambuf;
// 包装成 std::istream
class CustomInputStream : public std::istream {

public:
    CustomInputStream(const std::wstring& path);
    ~CustomInputStream();
    
private:
    
    std::unique_ptr<CustomInputStreambuf> buf;
};



class SequenceRun{


public:
    SequenceRun();
    ~SequenceRun();
    void Run(std::function<void()>& func);
    class SequenceRunClass;
private:
    
    std::shared_ptr<SequenceRunClass> pImpl;
};







#endif // !_MYSERVERAPI
