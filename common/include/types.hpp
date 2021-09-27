#ifndef INCLUDE_TYPES_HPP_
#define INCLUDE_TYPES_HPP_

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "spdlog/spdlog.h"

using string = std::string;

template <class K, class V>
using map = std::unordered_map<K, V>;

template <class K, class V, class H>
using hmap = std::unordered_map<K, V, H>;

template <class T>
using ordered_set = std::set<T>;

template <class T>
using set = std::unordered_set<T>;

template <class T>
using vector = std::vector<T>;

template <class T>
using queue = std::queue<T>;

template <class F, class S>
using pair = std::pair<F, S>;

using Address = std::string;

using Key = std::string;

using Bucket = std::string;

using Session = std::string;

using Value = std::string;

using logger = std::shared_ptr<spdlog::logger>;

#endif  // INCLUDE_TYPES_HPP_
