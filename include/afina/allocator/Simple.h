#ifndef AFINA_ALLOCATOR_SIMPLE_H
#define AFINA_ALLOCATOR_SIMPLE_H

#include <string>
#include <cstddef>

namespace Afina {
namespace Allocator {

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Pointer;
/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 * Allocator instance doesn't take ownership of wrapped memmory and do not delete it
 * on destruction. So caller must take care of resource cleaup after allocator stop
 * being needs
 */
// TODO: Implements interface to allow usage as C++ allocators
class FreeMemory {
public:
    FreeMemory(void *base, size_t size);
//    void move_free_ptr(void *new_ptr);
    void move_free_ptr(size_t new_ptr);
    void *get_free_ptr();
    void increase_available_now(size_t new_size);
    void decrease_available_now(size_t new_size);
    size_t get_available_now();
    void increase_all_free(size_t new_size);
    void decrease_all_free(size_t new_size);
    size_t get_all_free();

private:
    void *_ptr;
    size_t _available_now;
    size_t _all_free;
};

class Table : public FreeMemory{
public:
    Table (void *base, size_t size);
    void* write (void *ptr);
    void remove (void **ptr);

private:
    size_t _size_table;
    size_t _available_table;
    size_t **_back;

};

class Simple : public Table {
public:
    Simple(void *base, const size_t size);

    /**
     * TODO: semantics
     * @param N size_t
     */
    Pointer alloc(size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer &p, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer &p);

    /**
     * TODO: semantics
     */
    void defrag();

    /**
     * TODO: semantics
     */
    std::string dump() const;

private:
    void *_base;
    const size_t _base_len;
    //size_t **_back;
};

} // namespace Allocator
} // namespace Afina
#endif // AFINA_ALLOCATOR_SIMPLE_H
