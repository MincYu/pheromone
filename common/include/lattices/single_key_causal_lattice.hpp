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

#ifndef INCLUDE_LATTICES_SINGLE_KEY_CAUSAL_LATTICE_HPP
#define INCLUDE_LATTICES_SINGLE_KEY_CAUSAL_LATTICE_HPP

#include "core_lattices.hpp"

using VectorClock = MapLattice<string, MaxLattice<unsigned>>;

template <typename T>
struct VectorClockValuePair {
  VectorClock vector_clock;
  T value;

  VectorClockValuePair<T>() {
    vector_clock = VectorClock();
    value = T();
  }

  // need this because of static cast
  VectorClockValuePair<T>(unsigned) {
    vector_clock = VectorClock();
    value = T();
  }

  VectorClockValuePair<T>(VectorClock vc, T v) {
    vector_clock = vc;
    value = v;
  }

  unsigned size() {
    return vector_clock.size().reveal() * 2 * sizeof(unsigned) +
           value.size().reveal();
  }
};

template <typename T>
class SingleKeyCausalLattice : public Lattice<VectorClockValuePair<T>> {
 protected:
  void do_merge(const VectorClockValuePair<T> &p) {
    VectorClock prev = this->element.vector_clock;
    this->element.vector_clock.merge(p.vector_clock);

    if (this->element.vector_clock == p.vector_clock) {
      this->element.value.assign(p.value);
    } else if (!(this->element.vector_clock == prev)) {
      this->element.value.merge(p.value);
    }
  }

 public:
  SingleKeyCausalLattice() :
      Lattice<VectorClockValuePair<T>>(VectorClockValuePair<T>()) {}
  SingleKeyCausalLattice(const VectorClockValuePair<T> &p) :
      Lattice<VectorClockValuePair<T>>(p) {}
  MaxLattice<unsigned> size() { return {this->element.size()}; }
};

#endif  // INCLUDE_LATTICES_SINGLE_KEY_CAUSAL_LATTICE_HPP
