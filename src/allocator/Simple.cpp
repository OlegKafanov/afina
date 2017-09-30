#include <afina/allocator/Simple.h>

#include <afina/allocator/Pointer.h>
#include <afina/allocator/Error.h>

#define HEAD 2

namespace Afina {
namespace Allocator {

FreeMemory::FreeMemory(void *base, size_t size): _ptr(base), _available_now(size), _all_free(size){}

Table::Table(void *base, size_t size):_back(reinterpret_cast <size_t **> ((size_t *) base + size/sizeof(size_t) - 1)), _size_table(0), FreeMemory(base, size){}

Simple::Simple(void *base, size_t size) : _base(base), _base_len(size), Table(base, size){}

//void FreeMemory::move_free_ptr(void *new_ptr)
void FreeMemory::move_free_ptr(size_t new_ptr)
{
//    _ptr = new_ptr;
    _ptr = static_cast <size_t *>(_ptr) + new_ptr;
}
void FreeMemory::set_free_ptr(void* new_ptr)
{
    _ptr =  new_ptr;
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
    if (new_size > _available_now)
        _available_now = 0;
    else
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
    if (new_size > _all_free)
        _all_free = 0;
    else
        _all_free -= new_size;
}
size_t FreeMemory::get_all_free()
{
    return _all_free;
}

void** Table::write (void *ptr)
{
    size_t ** tmp_ptr;
    void ** tmp_ptr_void;

    if(!_size_table) //init
      {
        _size_table = 10;
        _available_table = _size_table;
        this->decrease_all_free(_size_table * sizeof(size_t));
        this->decrease_available_now(_size_table * sizeof(size_t));
        for (size_t i = 0; i < _size_table; i++)
            *(_back - i) = nullptr;
      }

    else if ((!_available_table) && (this->get_available_now() >= this->_size_table))
      {
        _available_table = _size_table;
        this->decrease_all_free(_size_table  * sizeof(size_t));
        this->decrease_available_now(_size_table * sizeof(size_t));
        for (size_t i = 0; i < _size_table; i++)
            *(_back - _size_table - i) = 0;
        _size_table = _size_table * 2;
      };

    if ((!_available_table) && this->get_available_now() < _size_table)
        throw AllocError(AllocErrorType::NoMemory, "NoMemory");

    for (size_t i = 0; i < _size_table; i++)  //||(!(*(_back - i)))
    {
        if(*(_back - i) == nullptr)
          {
            //tmp_ptr = static_cast <size_t **>(_back) - i;
            tmp_ptr = _back - i;
            *tmp_ptr = static_cast <size_t *> (ptr);
            //*(tmp_ptr) = (size_t *) (ptr);
            _available_table -= 1;
            break;
          }
    };
    //return (void *) (*tmp_ptr);
    tmp_ptr_void = reinterpret_cast<void**> (tmp_ptr);
    return tmp_ptr_void;
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
void Simple::move (size_t * ptr, size_t * move_ptr, size_t N)
{
    for (size_t i = 1; i < N + HEAD; i++)
    {
        *(move_ptr + i) = *(ptr + i);
    }
    *(reinterpret_cast <size_t **> (move_ptr )) = *(reinterpret_cast <size_t **> (ptr ));
}

Pointer Simple::alloc(size_t N)
{
    void **p;
    size_t M;

    if (N > this->get_all_free())
        throw AllocError(AllocErrorType::NoMemory, "NoMemory");
    else if (N > this->get_available_now())
        defrag();
    //(char) N -----> size_t M
    M = N / sizeof (size_t) + (( !(N % sizeof (size_t))) ? 0 : 1);
    p = this->write (static_cast <size_t *> (this->get_free_ptr()) + HEAD);
    //head:
    *(static_cast <size_t *> (this->get_free_ptr()) + 1) = M;//???????????  DEBAGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
    *(static_cast <size_t ***> (this->get_free_ptr())) = reinterpret_cast <size_t **> (p);
    //change free memory
    this->move_free_ptr (M + HEAD);
    this->decrease_available_now ((M + HEAD) * sizeof(size_t));
    this->decrease_all_free ((M + HEAD) * sizeof(size_t));

    return Pointer(p);
}

/**
 * TODO: semantics
 * @param p Pointer
 * @param N size_t
 */
void Simple::realloc(Pointer &p, size_t N)
{
    Pointer tmp_pointer;
    size_t M = N / sizeof (size_t) + (( !(N % sizeof (size_t))) ? 0 : 1);

//    if (!p._ptr)
//        throw AllocError(AllocErrorType::InvalidFree, "SEGFAULT");
    if (!p._ptr)
    {
        p = alloc(N);
        return;
    }

    size_t * ptr = static_cast <size_t *>(p.get());

    if (N <= *(ptr - 1) * sizeof (size_t))
    {
        this->increase_all_free (*(ptr - 1) - N / sizeof (size_t));
        *(ptr - 1) = *(ptr - 1) - N / sizeof (size_t);
    }
    else if (N > this->get_available_now())
    {
        throw AllocError(AllocErrorType::NoMemory, "NoMemory");
    }
    else
    {
            if (ptr + *(ptr - 1) == this->get_free_ptr())
        {
            *(ptr - 1) = *(ptr - 1) + M;
            this->decrease_available_now(M);
            this->decrease_all_free(M);
            this->move_free_ptr(M);
        }
        else
        {
            tmp_pointer = alloc(N);
            move (ptr, static_cast <size_t *> (tmp_pointer.get()), *(ptr - 1));
            *(ptr - 1) = N;
            free(p);
        }
    }
    if (tmp_pointer.get())
        p = tmp_pointer;
}

/**
 * TODO: semantics
 * @param p Pointer
 */
void Simple::free(Pointer &p)
{
    //if (p._ptr == NULL) {
    //   throw AllocError(AllocErrorType::InvalidFree, "Null pointer");
    //}
    size_t *ptr;
    //*p =nullptr;
    ptr = static_cast <size_t *> (p.get());
    this->increase_all_free (*(ptr - 1) * sizeof(size_t));
//    this->increase_all_free (*(static_cast<size_t*> (*p._ptr) + 1));
    *(ptr - 2) = 0;
    p._ptr = nullptr;
//p._ptr = 0;
    p.~Pointer();
    //  ** (reinterpret_cast <size_t **> (p._ptr)) = 0;
    //  this->remove(p._ptr);
}

/**
 * TODO: semantics
 */
void Simple::defrag()
{
    size_t *ptr = nullptr;
    size_t *move_ptr = nullptr;
    size_t N;

    ptr = static_cast <size_t *> (_base);

    while (ptr < this->get_free_ptr()) // !!!!!!!!!!!!!!!!!!!!
    {
        N = *(ptr + 1);
        if (!(*ptr) && !move_ptr)
        {
            //move_ptr = reinterpret_cast <size_t *>(&ptr);
            move_ptr = *reinterpret_cast <size_t **>(ptr);
        }
        if (*ptr && move_ptr)
        {
            move (ptr, move_ptr, N);
            //*(static_cast <size_t **> (ptr)) = move_ptr;
            *(reinterpret_cast <size_t **> (move_ptr)) = *(reinterpret_cast <size_t **> (ptr));
            move_ptr = move_ptr + N + HEAD;
        }
        ptr = ptr + N + HEAD;
    }

    if (move_ptr)
        this->set_free_ptr (move_ptr);
    this->increase_available_now (this->get_all_free() - this->get_available_now());
}

/**
 * TODO: semantics
 */
std::string Simple::dump() const { return ""; }

} // namespace Allocator
} // namespace Afina
