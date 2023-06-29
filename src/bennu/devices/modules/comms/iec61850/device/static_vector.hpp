/**
   static_vector defines an STL compliant container that exposes an std::vector
   API while maintaining a static sized internal data store.  This may be useful
   when dealing with embedded systems or other applications requiring a staticly
   sized sequential data store.
*/
#ifndef __CCSS_PROTOCOLS_UTILITY_STATIC_VECTOR__
#define __CCSS_PROTOCOLS_UTILITY_STATIC_VECTOR__

/* stl includes */
#include <cstddef>
#include <cstdint>
#include <memory>

#ifndef EMBEDDED
using std::addressof;
#endif // EMBEDDED

namespace ccss_protocols {
  namespace utility {
    template <typename T, size_t CAPACITY>
    class static_vector {
    public:

      typedef T 	    		value_type;
      typedef T*		        pointer;
      typedef const T*	        const_pointer;
      typedef value_type&	        reference;
      typedef const value_type&   const_reference;
      typedef value_type*	        iterator;
      typedef const value_type*	const_iterator;
      typedef std::size_t		size_type;
      typedef std::ptrdiff_t	difference_type;

      static_vector() : size_(0), data_() {}

      /* capacity */
      inline size_type size(void) const { return size_; }
      inline size_type max_size(void) const { return CAPACITY; }
      inline size_type capacity(void) const { return CAPACITY; }
      inline bool empty(void) const { return size() == 0; }
      inline bool full(void) const { return (size() == CAPACITY-1); }

      /* modifiers */
      void push_back(T val) {
    if (size_  < CAPACITY) {
      data_[size_] = val;
      ++size_;
    }
      }

      inline void clear(void) { size_ = 0;}

      void pop_back(void) {
    if (size_ > 0)
      --size_;
      }

      void resize( size_type new_size ) {
    if ( new_size < CAPACITY )
      size_ = new_size;
      }

      /* accessors */
      inline reference front(void) { return data_[0]; }
      inline const_reference front(void) const { return data_[0]; }

      inline reference back(void) { return data_[size_-1]; }
      inline const_reference back(void) const { return data_[size_-1]; }

      inline reference operator[](size_t pos) { return data_[pos]; }
      inline const_reference operator[](size_t pos) const { return data_[pos]; }

      inline pointer data(void) { return data_; }
      inline const_pointer data(void) const { return data_; }

      /* iterators */
      iterator begin(void) { addressof(data_[0]); }
      iterator end(void) { addressof(data_[size_]); }

      const_iterator cbegin(void) const { addressof(data_[0]); }
      const_iterator cend(void) const { addressof(data_[size_]); }

      /* comparisons */
      bool operator==(static_vector<T, CAPACITY> const& rhs) const {
    if (size_ == rhs.size()) {
      for ( size_t i=0; i < rhs.size(); ++i ) {
        if ( data_[i] != rhs[i] ) { return false; }
      } return true;
    } return false;
      }

      bool operator!=(static_vector<T, CAPACITY> const& rhs) const {
    if (size_ == rhs.size()) {
      for ( size_t i=0; i < rhs.size(); ++i ) {
        if ( data_[i] != rhs[i] ) { return true; }
      } return false;
    } return true;
      }

    private:
      size_type size_; /**<number of elements currently in the static_vector*/
      T data_[CAPACITY];
    };
  } // utility
} // ccss_protocols

#endif /* __CCSS_PROTOCOLS_UTILITY_STATIC_VECTOR__ */
