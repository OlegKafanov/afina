#include <afina/allocator/Pointer.h>
#include <stdlib.h>

namespace Afina {
namespace Allocator {

Pointer::Pointer():_ptr(nullptr){}
Pointer::Pointer(void **ptr): _ptr(ptr){}

Pointer::Pointer(const Pointer &p) {
    this->_ptr = p._ptr;
}
Pointer::Pointer(Pointer &&p) {
    this->_ptr = p._ptr;
}

Pointer &Pointer::operator=(const Pointer &p) {
    this->_ptr = p._ptr;
    return *this;
}
Pointer &Pointer::operator=(Pointer &&p) {
    this->_ptr = p._ptr;
    return *this;
}

void* Pointer::get() const {
  if (*_ptr) {
    return *_ptr;
  }
  else {
    return NULL;
  }
}
} // namespace Allocator
} // namespace Afina
