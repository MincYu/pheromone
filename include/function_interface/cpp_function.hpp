#ifndef INCLUDE_CPP_FUNCTION_HPP_
#define INCLUDE_CPP_FUNCTION_HPP_

using string = std::string;

class EpheObject {
  public:
    virtual void* get_value() = 0;
    virtual void set_value(const void* value, size_t val_size) = 0;
    virtual void update_size(size_t size) = 0;
    virtual size_t get_size() = 0;
};

class UserLibraryInterface {
  public:
    virtual EpheObject* create_object(size_t size = 1024 * 1024) = 0;
    virtual EpheObject* create_object(string target_function, bool many_to_one_trigger = true, size_t size = 1024 * 1024) = 0;
    virtual EpheObject* create_object(string bucket, string key, size_t size = 1024 * 1024) = 0;
    
    virtual void send_object(EpheObject *data, bool output = false) = 0;
    virtual EpheObject* get_object(string bucket, string key, bool from_ephe_store=true) = 0;

    virtual string gen_unique_key() = 0;
    virtual size_t get_size_of_arg(int arg_idx) = 0;
};

extern "C" {
  int handle(UserLibraryInterface* library, int arg_size, char** arg_values);
}

#endif  // INCLUDE_CPP_FUNCTION_HPP_
