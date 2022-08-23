// Copyright (C) 2022 Maximilian Reininghaus
// Released under MIT License
// license available in LICENSE file, or at
// http://www.opensource.org/licenses/mit-license.php

#pragma once

#include <iterator>
#include <type_traits>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/stl_interfaces/view_interface.hpp>

namespace cnpypp {
/*
template <typename TValueType>
class stride_iterator : public
boost::iterator_facade<stride_iterator<TValueType>, TValueType,
std::random_access_iterator_tag> { public: using value_type = typename
boost::iterator_facade<stride_iterator<TValueType>, TValueType,
std::random_access_iterator_tag>::value_type;

  stride_iterator(std::byte* ptr, std::ptrdiff_t stride) : ptr_{ptr},
stride_{stride} {}

  friend class boost::iterator_core_access;

  using reference_type = std::add_lvalue_reference_t<TValueType>;

  void increment() { ptr_ += stride_; }

  void decrement() { ptr_ -= stride_; }

  void advance(size_t n) { ptr_ += n * stride_; }

  bool equal(stride_iterator const& other) {
    return ptr_ == other.ptr_;
  }

  std::ptrdiff_t distance_to(stride_iterator const& other) const {
    return (other.ptr_ - ptr_) / stride_;
  }

  reference_type dereference() const {
      return *reinterpret_cast<value_type*>(ptr_);
  }

private:
    std::byte* ptr_;
    std::ptrdiff_t const stride_;
};
*/

template <typename TValueType>
class stride_iterator : public boost::stl_interfaces::iterator_interface<
                            stride_iterator<TValueType>,
                            std::random_access_iterator_tag, TValueType> {
public:
  using value_type = TValueType;

  stride_iterator(std::byte* ptr, std::ptrdiff_t stride)
      : ptr_{ptr}, stride_{stride} {}
  stride_iterator() : ptr_{nullptr}, stride_{} {}

  // friend class boost::iterator_core_access;

  using reference_type = std::add_lvalue_reference_t<TValueType>;

  stride_iterator& operator+=(size_t n) {
    ptr_ += n * stride_;
    return *this;
  }

  reference_type operator*() const {
    return *reinterpret_cast<value_type*>(ptr_);
  }

  bool operator==(stride_iterator const& other) const {
    return ptr_ == other.ptr_;
  }

private:
  std::byte* ptr_;
  std::ptrdiff_t const stride_;
};

template <typename Iterator, typename Sentinel = Iterator>
struct subrange
    : boost::stl_interfaces::view_interface<subrange<Iterator, Sentinel>> {
  subrange() = default;
  constexpr subrange(Iterator it, Sentinel s) : first_(it), last_(s) {}

  constexpr auto begin() const { return first_; }
  constexpr auto end() const { return last_; }

private:
  Iterator first_;
  Sentinel last_;
};
} // namespace cnpypp
