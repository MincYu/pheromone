//  Copyright 2019 U.C. Berkeley RISE Lab
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef INCLUDE_LATTICES_MULTI_KEY_CAUSAL_LATTICE_HPP
#define INCLUDE_LATTICES_MULTI_KEY_CAUSAL_LATTICE_HPP

#include "core_lattices.hpp"

using VectorClock = MapLattice<string, MaxLattice<unsigned>>;

template <typename T>
struct MultiKeyCausalPayload {
  VectorClock vector_clock;
  MapLattice<Key, VectorClock> dependencies;
  T value;

  MultiKeyCausalPayload<T>() {
    vector_clock = VectorClock();
    dependencies = MapLattice<Key, VectorClock>();
    value = T();
  }

  // need this because of static cast
  MultiKeyCausalPayload<T>(unsigned) {
    vector_clock = VectorClock();
    dependencies = MapLattice<Key, VectorClock>();
    value = T();
  }

  MultiKeyCausalPayload<T>(VectorClock vc, MapLattice<Key, VectorClock> dep,
                           T v) {
    vector_clock = vc;
    dependencies = dep;
    value = v;
  }

  unsigned size() {
    unsigned dep_size = 0;
    for (const auto &pair : dependencies.reveal()) {
      dep_size += pair.first.size();
      dep_size += pair.second.size().reveal() * 2 * sizeof(unsigned);
    }
    return vector_clock.size().reveal() * 2 * sizeof(unsigned) + dep_size +
           value.size().reveal();
  }
};

template <typename T>
class MultiKeyCausalLattice : public Lattice<MultiKeyCausalPayload<T>> {
 protected:
  void do_merge(const MultiKeyCausalPayload<T> &p) {
    VectorClock prev = this->element.vector_clock;
    this->element.vector_clock.merge(p.vector_clock);

    if (this->element.vector_clock == p.vector_clock) {
      // incoming version is dominating
      this->element.dependencies.assign(p.dependencies);
      this->element.value.assign(p.value);
    } else if (!(this->element.vector_clock == prev)) {
      // versions are concurrent
      this->element.dependencies.merge(p.dependencies);
      this->element.value.merge(p.value);
    }
  }

 public:
  MultiKeyCausalLattice() :
      Lattice<MultiKeyCausalPayload<T>>(MultiKeyCausalPayload<T>()) {}
  MultiKeyCausalLattice(const MultiKeyCausalPayload<T> &p) :
      Lattice<MultiKeyCausalPayload<T>>(p) {}
  MaxLattice<unsigned> size() { return {this->element.size()}; }
};

#endif  // INCLUDE_LATTICES_MULTI_KEY_CAUSAL_LATTICE_HPP
