#include "http_form.h"

namespace curlion {

static std::vector<curl_forms> CreateOptions(const std::shared_ptr<HttpForm::Part>& part);
    
HttpForm::~HttpForm() {
    
    curl_formfree(handle_);
}
    
    
std::error_condition HttpForm::AddPart(const std::shared_ptr<Part>& part) {
    
    std::error_condition error;
    
    auto options = CreateOptions(part);
    auto result = curl_formadd(&handle_, &handle_last_, CURLFORM_ARRAY, options.data(), CURLFORM_END);
    
    if (result == CURL_FORMADD_OK) {
        parts_.push_back(part);
    }
    else {
        error.assign(result, CurlFormErrorCategory());
    }
    
    return error;
}
    
    
static std::vector<curl_forms> CreateOptions(const std::shared_ptr<HttpForm::Part>& part) {
    
    std::vector<curl_forms> options;
    
    curl_forms name_option;
    name_option.option = CURLFORM_PTRNAME;
    name_option.value = part->name.c_str();
    options.push_back(name_option);
    
    const auto& content = part->content;
    if ((! content.empty()) || (part->files.empty())) {
        
        curl_forms content_option;
        content_option.option = CURLFORM_PTRCONTENTS;
        content_option.value = content.c_str();
        options.push_back(content_option);
        
        curl_forms content_length_option;
        content_length_option.option = CURLFORM_CONTENTSLENGTH;
        content_length_option.value = reinterpret_cast<const char*>(content.length());
        options.push_back(content_length_option);
    }
    else {
        
        for (const auto& each_file : part->files) {
            
            curl_forms path_option;
            path_option.option = CURLFORM_FILE;
            path_option.value = each_file->path.c_str();
            options.push_back(path_option);
            
            if (! each_file->name.empty()) {
                curl_forms name_option;
                name_option.option = CURLFORM_FILENAME;
                name_option.value = each_file->name.c_str();
                options.push_back((name_option));
            }
            
            if (! each_file->content_type.empty()) {
                curl_forms content_type_option;
                content_type_option.option = CURLFORM_CONTENTTYPE;
                content_type_option.value = each_file->content_type.c_str();
                options.push_back(content_type_option);
            }
        }
    }
    
    curl_forms end_option;
    end_option.option = CURLFORM_END;
    end_option.value = nullptr;
    options.push_back(end_option);

    return options;
}
    
}