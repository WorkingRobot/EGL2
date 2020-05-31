#pragma once

#include <memory>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "error.h"

namespace curlion {

/**
 HttpForm used to build a multi-part form request content.
 
 The instance of this class is used in HttpConnection::SetRequestForm method.
 */
class HttpForm {
public:
    /**
     File represents information of a file in a part.
     */
    class File {
    public:
        /**
         Construct a empty file.
         */
        File() { }
        
        /**
         Construct a file with specified path.
         
         @param path
             Path of the file.
         */
        File(const std::string& path) : path(path) { }
        
        /**
         Path of the file.
         */
        std::string path;
        
        /**
         Name of the file.
         
         If this field is empty, the name in path would be used.
         */
        std::string name;
        
        /**
         Content type of the file.
         
         If this field is empty, the content type is determinated by the content of file.
         */
        std::string content_type;
    };
    
    /**
     Part represents information of a single part in a multi-part form.
     */
    class Part {
    public:
        /**
         Construct an empty part.
         */
        Part() { }
        
        /**
         Construct a part with specified name and content.
         
         @param name
             Name of the part.
         
         @param content
             Content of the part.
         */
        Part(const std::string& name, const std::string& content) : name(name), content(content) { }
        
        /**
         Name of the part.
         */
        std::string name;
        
        /**
         Content of the part.
         */
        std::string content;
        
        /**
         Files in the part.
         
         Note that files and content can not coexist in a part, and content has a higher priority
         than files. That is, if content is not empty, all files are ignored.
         */
        std::vector<std::shared_ptr<File>> files;
    };
    
public:
    /**
     Destruct the HttpForm instance.
     */
    ~HttpForm();
    
    /**
     Add a part to the form.
     
     @param part
         The part is added, must not be nullptr.
     
     @return 
         Return an error on failure.
     */
    std::error_condition AddPart(const std::shared_ptr<Part>& part);
    std::error_condition AddPart(const Part&& part);
    std::error_condition AddPart(const std::string& name, const std::string& content);
    
    /**
     Get the handle of the form.
     
     @return
         The curl_httppost* handle of the form.
     */
    curl_httppost* GetHandle() const {
        return handle_;
    }
    
private:
    curl_httppost* handle_ = nullptr;
    curl_httppost* handle_last_ = nullptr;
    
    std::vector<std::shared_ptr<Part>> parts_;
};
    
}