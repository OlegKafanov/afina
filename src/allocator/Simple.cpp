#include <afina/allocator/Simple.h>

#include <afina/allocator/Pointer.h>
#include <afina/allocator/Error.h>

#define HEAD 2

namespace Afina {
namespace Allocator {

FreeMemory::FreeMemory(void *base, size_t size): _ptr(base), _available_now(size), _all_free(size){}

Table::Table(void *base, size_t size):_back(reinterpret_cast <size_t **> ((size_t *) base + size/sizeof(size_t) - 1)), FreeMemory(base, size){}

Simple::Simple(void *base, size_t size) : _base(base), _base_len(size), Table(base, size){}

void FreeMemory::move_free_ptr(void *new_ptr)
{
    _ptr = new_ptr;
}
void *FreeMemory::get_free_ptr()
{
    return _ptr;
}
void FreeMemory::increase_available_now(size_t new_size)
{
    _available_now += new_size;
}
void FreeMemory::decrease_available_now(size_t new_size)
{
    _available_now -= new_size;
}
size_t FreeMemory::get_available_now()
{
    return _available_now;
}
void FreeMemory::increase_all_free(size_t new_size)
{
    _all_free += new_size;
}
void FreeMemory::decrease_all_free(size_t new_size)
{
    _all_free -= new_size;
}
size_t FreeMemory::get_all_free()
{
    return _all_free;
}

void** Table::write (void *ptr)
{
    size_t ** tmp_ptr;

    if(!_size_table) //init
      {
        _size_table = 10;
        _available_table = _size_table;
        this->decrease_all_free(_size_table);
        this->decrease_available_now(_size_table);
        for (size_t i = 0; i < _size_table; i++)
            *(_back - i) = nullptr;
      }

    else if ((!_available_table) && (this->get_available_now() >= this->_size_table))
      {
        _available_table = _size_table;
        this->decrease_all_free(_size_table);
        this->decrease_available_now(_size_table);
        for (size_t i = 0; i < _size_table; i++)
            *(_back - _size_table - i) = 0;
        _size_table = _size_table * 2;
      };

//    if (this->get_available_now() < _size_table)
//        throw AllocError(AllocErrorType::NoMemory, "NoMemory");

    for (size_t i = 0; (i < _size_table)||(!(_back + i)); i++)
        if(*(_back + i) == nullptr)
          {
            tmp_ptr =_back + i;
            //*tmp_ptr = static_cast <size_t *> (ptr);
            *tmp_ptr = (size_t *) (ptr);
            _available_table -= 1;
          }

    return (void **) (tmp_ptr);
}

void Table::remove (void **ptr)
{
    *ptr = nullptr;//???????????????
    _available_table += 1;
}

/**
 * TODO: semantics
 * @param N size_t
 */
Pointer Simple::alloc(size_t N)
{
    void **p;

    if (N > this->get_all_free())
  ;//      throw AllocError(AllocErrorType::NoMemory, "NoMemory");
    else if (N > this->get_available_now())
        ;//defrag!!!!!!!!!!!!!!!!!!!!;

    //p = this->write (static_cast <size_t *> (this->get_free_ptr()) + HEAD);
    *p = _base;
    //head:
    *(static_cast <size_t *> (this->get_free_ptr()) + 1) = N;
    *(static_cast <void **> (this->get_free_ptr())) = p;//???????????
    //change free memory
    this->move_free_ptr (static_cast <size_t *> (this->get_free_ptr()) + N + HEAD);
    this->decrease_available_now (N + HEAD);
    this->decrease_all_free (N + HEAD);

    return Pointer(*p);
}

/**
 * TODO: semantics
 * @param p Pointer
 * @param N size_t
 */
void Simple::realloc(Pointer &p, size_t N)
{

}

/**
 * TODO: semantics
 * @param p Pointer
 */
void Simple::free(Pointer &p)
{
    ** (reinterpret_cast <size_t **> (p._ptr)) = 0;
    this->remove(p._ptr);
}

/**
 * TODO: semantics
 */
void Simple::defrag()
{

}

/**
 * TODO: semantics
 */
std::string Simple::dump() const { return ""; }

} // namespace Allocator
} // namespace Afina
