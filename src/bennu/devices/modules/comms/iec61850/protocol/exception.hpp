/**
   @brief exception class for the protocols library
 */
#ifndef __EXCEPTION_HPP__
#define __EXCEPTION_HPP__

/* system includes */
#include <exception>
#include <stdexcept>
#include <string>

namespace ccss_protocols {

  class Exception : public std::exception {
  public:
    virtual ~Exception() throw() {}

    Exception( std::string const& description ) :
      std::exception(), what_(description) {}

    virtual char const* what() const throw() {
      return (what_.c_str());
    }

  private:
    std::string what_;
  };

} // namespace ccss_protocols

#endif /* __EXCEPTION_HPP__ */
