#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value)
{
    if (_backend.find(key) != _backend.end())
    {
        Set (key, value);
    }

    if (_backend.size() >= _max_size)
    {
        _backend.erase(_keys.front());
        _keys.pop_front();
    }

    _backend.insert (_backend.end(), std::pair<std::string, std::string> (key, value));
    update_key(key);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value)
{
    if (_backend.find(key) == _backend.end())
    {
        this->Put(key, value);
        return true;
    }
//    std::unique_lock<std::mutex> guard(_lock);
    return false;
}


// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value)
{
    if (_backend.find(key) == _backend.end())
    {
        return false;
    }
    else
    {
        _backend.erase(key);
        _backend.insert (_backend.end(), std::pair<std::string, std::string> (key, value));
        update_key(key);
        return true;
    }

    std::unique_lock<std::mutex> guard(_lock);
    return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key)
{
    if (_backend.find(key) == _backend.end())
    {
        return false;
    }
    else
    {
        _backend.erase(key);
        _keys.remove(key);
        return true;
    }
    std::unique_lock<std::mutex> guard(_lock);
    return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const
{
    std::map<std::string, std::string>::const_iterator it;
    it = this->_backend.find(key);

    if (it == _backend.end())
    {
        return false;
    }
    else
    {
        value = it->second;
//        update_key(key);
        return true;
    }
    std::unique_lock<std::mutex> guard(*const_cast<std::mutex *>(&_lock));
    return false;
}

void MapBasedGlobalLockImpl::update_key(const std::string &key)
{
    _keys.remove(key);
    _keys.push_back(key);
}

} // namespace Backend
} // namespace Afina
