#include <afina/allocator/Pointer.h>

namespace Afina {
namespace Allocator {

Pointer::Pointer():_ptr(nullptr){}
Pointer::Pointer(void *ptr): _ptr(&ptr){}

Pointer::Pointer(const Pointer &) {}
Pointer::Pointer(Pointer &&) {}

Pointer &Pointer::operator=(const Pointer &) { return *this; }
Pointer &Pointer::operator=(Pointer &&) { return *this; }

} // namespace Allocator
} // namespace Afina
