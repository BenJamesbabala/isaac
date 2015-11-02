#ifndef ISAAC_TYPES_H
#define ISAAC_TYPES_H

#include <algorithm>
#include <vector>
#include <cstddef>
#include "isaac/defines.h"

namespace isaac
{

typedef long long int_t;

class shape_t
{
    friend std::ostream& operator<<(std::ostream & oss, shape_t const &);
public:
    shape_t(std::vector<int_t> const & list): data_(list){}
    shape_t(std::initializer_list<int_t> const & list) : data_(list){}
    shape_t(int_t a) : data_{a} {}
    shape_t(int_t a, int_t b) : data_{a, b} {}

    size_t size() const { return data_.size(); }
    int_t max() const   { return std::accumulate(data_.begin(), data_.end(), std::numeric_limits<int_t>::min(), [](int_t a, int_t b){ return std::max(a, b); }); }
    int_t min() const   { return std::accumulate(data_.begin(), data_.end(), std::numeric_limits<int_t>::max(), [](int_t a, int_t b){ return std::min(a, b); }); }
    int_t prod() const  { return std::accumulate(data_.begin(), data_.end(), 1, std::multiplies<int>()); }

    int_t front() const { return data_.front(); }
    int_t back() const { return data_.back(); }

    int_t& operator[](size_t i) { return data_[i]; }
    int_t operator[](size_t i) const { return data_[i]; }

    operator std::vector<int_t>() const { return data_; }
private:
    std::vector<int_t> data_;
};

inline ISAACAPI std::ostream& operator<<(std::ostream & oss, shape_t const &shape)
{
  for(int_t x: shape.data_)
    oss << x << ',';
  return oss;
}

static const int_t start = 0;
static const int_t end = -1;
struct slice
{
//  slice(int_t _start) : start(_start), end(_start + 1), stride(1){}
  slice(int_t _start, int_t _end, int_t _stride = 1) : start(_start), end(_end), stride(_stride) { }

  int_t size(int_t bound) const
  {
    int_t effective_end = (end < 0)?bound - (end + 1):end;
    return (effective_end - start)/stride;
  }

  int_t start;
  int_t end;
  int_t stride;
};
static const slice all = slice(start, end, 1);

}
#endif
