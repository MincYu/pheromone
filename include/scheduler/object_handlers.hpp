#ifndef INCLUDE_OBJ_HANDLERS_HPP_
#define INCLUDE_OBJ_HANDLERS_HPP_

#include <signal.h>
#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstddef>
#include <atomic>
#include "libipc/ipc.h"
#include "libipc/shm.h"
#include "capo/random.hpp"
#include "common.hpp"
#include "trigger.hpp"

void copy_to_shm_obj(string &key_name, const char* data_src, unsigned data_size);

pair<char*, unsigned> get_shm_obj(string &key_name);

void check_object_arrival(logger log, BucketKey &bucket_key, map<Bucket, 
                            vector<TriggerPointer>> &bucket_triggers_map, vector<string> &active_triggers, 
                            vector<TriggerFunctionMetadata> &active_func_metadata);

// void check_re_execution();

// void reclaim_object();

// void notify_func_start();


#endif
