#include <iostream>
#include <cassert>
#include <time.h>
#include <variant>
#include "cache.hh"
#include "evictor.hh"

// CONSTANTS
int REQUESTCOUNT = 10000;
int GETPROB = 67;
int SETPROB = 98;   // SETPROB is 100 - (desired prob). It's the top of the range.
                    // Don't need a DELPROB, it's just in an else statement.
std::string HOST = "127.0.0.1";
std::string PORT = "3618";

// global_variables
double total_gets = 0.;     // Effectively an int, but typing for division later.
int get_hits = 0;

std::vector<std::string> keys_in_use;
std::vector<int> key_touch_count;

/*

*/

void 
cache_set(Cache& items, key_type name, Cache::val_type data, Cache::size_type size)
{
    std::cout << "Attempting to add item of size " << size << " ...\n";
    /* Create an item with key 'name', value 'data', and size 'size'. Add it to the cache. */
    //Cache::val_type val = data;
    items.set(name, data, size);
}

void 
cache_get(Cache& items, key_type key)
{
    std::cout << "Getting item " << key << " from cache...\n";
    total_gets += 1;
    Cache::size_type got_item_size = 0;
    Cache::val_type got_item = items.get(key, got_item_size);
    if (got_item != nullptr) {
        get_hits += 1;
        std::cout << key << " gotten successfully\n";
    }
    else {
        std::cout << "Falied to get " << key << "\n";
    }
}

// 
void 
cache_del(Cache& items, key_type key)
{
    bool delete_success = items.del(key);
    if (delete_success) {
        std::cout << "Deleted " << key << " from the cache.\n";
    }
    else {
        std::cout << "Invalid delete on item " << key << "\n";
    }
}

// Helper for generate_request()
bool 
useRealKey(int fail_prob)
{
    int use_real_key_prob = rand() % 100;
    if (use_real_key_prob > fail_prob) {
        return true;
    }
    else {
        return false;
    }
}

// Helper for generate_request(), specifically when setting
std::string
generate_random_data(int length){
    std::string data = "";
    while(length > 1) {                          //TODO: Check for null-termination
        char char_ascii = rand() % 26 + 97;     //Outputs something between 'a' and 'z'.
        data.push_back(char_ascii);
        length = length - 1;
    }
    //std::cout << data << "\n";
    std::string test_data(data);
    //std::cout << test_data << "\n";
    return test_data;
}

// Helper for generate_request()
std::string
generate_random_key(int length){
    std::string data = "";
    while(length > 1) {                          //TODO: Check for null-termination
        char char_ascii = rand() % 26 + 65;     //Outputs something between 'a' and 'z'.
        data.push_back(char_ascii);
        length = length - 1;
    }
    //std::cout << data << "\n";
    std::string test_data(data);
    //std::cout << test_data << "\n";
    return test_data;
}

// Helper for generate_request()
key_type
select_key() {
    int rand_index = rand() % keys_in_use.size();
    key_type active_key = keys_in_use[rand_index];
    key_touch_count[rand_index] += 1;
    return active_key;
}

// Generate a random request for the cache
std::vector<std::variant<std::string, int>> 
generate_request()
{
    std::cout << "Generating new request!\n";
    std::vector<std::variant<std::string, int>> random_request;
    int op_prob = rand() % 100;
    /*
    Probability Details:
    Total range: 0-99
    0 <-> (GETPROB - 1): Get
    GETPROB <-> (SETPROB - 1): Delete
    SETPROB <-> 99: Set
    */
    std::cout << "Choosing a request type...\n";
    if (op_prob < GETPROB) {
        // Get
        std::cout << "Making a get request!\n";
        int fail_prob = 0;
        if (useRealKey(fail_prob)){
            key_type active_key = select_key();

            random_request.push_back("get");
            random_request.push_back(active_key);

            return random_request;
        }
        else {
            // Return a request to get a dummy key (which will never exist in the cache)
            std::string dummy_key = "-A";

            random_request.push_back("get");
            random_request.push_back(dummy_key);

            return random_request;
        }
    }
    else if (op_prob > SETPROB) {       // Note that this is >, not <
        // Set
        std::cout << "Making a set request!\n";
        int data_length = 0;
        int size_prob = rand() % 100;
        if (size_prob < 85) {
            data_length = rand() % 6 + 2;
        }
        else if (size_prob < 98) {
            data_length = rand() % 35 + 10;
        }
        else {
            data_length = rand() % 15 + 100;
        }
        std::string rand_data = generate_random_data (data_length);
        

        int fail_prob = 2;
        if (useRealKey(fail_prob)) {
            // Select a key from our list to set the value of (frequency distribution?)
            key_type active_key = select_key();
            
            random_request.push_back("set");
            random_request.push_back(rand_data);
            random_request.push_back(active_key);
            random_request.push_back(data_length);

            return random_request;
        }
        else {
            // Return a request to set a brand new key, and add that key to our list.
            std::string new_key = generate_random_key(8);

            random_request.push_back("set");
            random_request.push_back(new_key);
            random_request.push_back(rand_data);
            random_request.push_back(data_length);

            keys_in_use.push_back(new_key);
            
            return random_request;
        }
    }
    else {
        // Delete
        std::cout << "Making a delete request!\n";
        int fail_prob = 75;
        if (useRealKey(fail_prob)) {
            // Select a key from our list to delete (frequency distribution?)
            key_type active_key = select_key();

            random_request.push_back("delete");
            random_request.push_back(active_key);

            return random_request;
        }
        else {
            // Return a request to delete a non-existant key
            std::string dummy_key = "-A";

            random_request.push_back("delete");
            random_request.push_back(dummy_key);

            return random_request;
        }
    }
}

void
cache_warmup(Cache& items) {
    // Set a bunch of values to the cache, to fill it up
    int data_length;
    for(int i = 0; i < 100; i++) {
        int size_prob = rand() % 100;
        if (size_prob < 95) {
            data_length = rand() % 6 + 2;
        }
        else if (size_prob < 99) {
            data_length = rand() % 35 + 10;
        }
        else {
            data_length = rand() % 15 + 100;
        }
        std::string test_data = generate_random_data (data_length);
        Cache::val_type rand_data = test_data.c_str();
        //std::cout << rand_data << "\n";

        key_type new_key = generate_random_key(10);

        cache_set(items, new_key, rand_data, data_length);

        keys_in_use.push_back(new_key);
        key_touch_count.push_back(1);
    }
    std::cout << "Warmup Complete\n";
    return;
}

int main() 
{
    srand (time(NULL));
    Cache items(HOST, PORT);
    std::cout << "Beginning request generation!\n";
    cache_warmup(items);
    for (int i = 0; i < REQUESTCOUNT; i++) {

        std::vector<std::variant<std::string, int>> new_req = generate_request();
        std::cout << "Made a request!\n";

        //std::cout << new_req << "\n";

        if (std::get<std::string>(new_req[0]) == "set") {
            assert(new_req.size() == 4 && "'set' request generated with the wrong number of elements!\n");
            // Vector contents: {"set", key_type key, val_type data, size_type size}
            Cache::val_type data = std::get<std::string>(new_req[1]).c_str();
            key_type key = std::get<std::string>(new_req[2]);
            Cache::size_type size = std::get<int>(new_req[3]);
            cache_set(items, key, data, size);
        }
        else if (std::get<std::string>(new_req[0]) == "get") {
            assert(new_req.size() == 2 && "'get' request generated with the wrong number of elements!\n");
            // Vector contents: {"get", key_type key}

            key_type key = std::get<std::string>(new_req[1]);
            cache_get(items, key);
        }
        else if (std::get<std::string>(new_req[0]) == "delete") {
            assert(new_req.size() == 2 && "'del' request generated with the wrong number of elements!\n");
            // Vector contents: {"get", key_type key}

            key_type key = std::get<std::string>(new_req[1]);
            cache_del(items, key);
        }
    }
    
    if (total_gets == 0) {
    std::cout << "There were no gets\n";
    }
    else {
    double get_hit_ratio = get_hits / total_gets;
    std::cout << "get() hit ratio: " << get_hit_ratio << "\n";
    }
    return 0;
}