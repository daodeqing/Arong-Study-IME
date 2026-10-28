#pragma once

#include <list>
#include <limits>
#include <optional>
#include <cstddef>
#include <unordered_map>

template <typename Value> struct CacheValueWeight
{
    size_t operator()(const Value &) const
    {
        return sizeof(Value);
    }
};

template <typename Key, typename Value, typename ValueWeight = CacheValueWeight<Value>> class CircularBuffer
{
  public:
    explicit CircularBuffer(size_t capacity, size_t max_weight = (std::numeric_limits<size_t>::max)())
        : _capacity(capacity), _max_weight(max_weight)
    {
    }

    void insert(const Key &key, const Value &value)
    {
        const size_t weight = ValueWeight{}(value);
        auto it = _map.find(key);
        if (it != _map.end())
        {
            _weight -= it->second.weight;
            _list.erase(it->second.position);
            _map.erase(it);
        }
        if (_capacity == 0 || weight > _max_weight)
            return;
        while (!_list.empty() && (_list.size() >= _capacity || _weight > _max_weight - weight))
        {
            const Key &oldest_key = _list.front();
            _weight -= _map.find(oldest_key)->second.weight;
            _map.erase(oldest_key);
            _list.pop_front();
        }

        _list.push_back(key);
        try
        {
            _map.emplace(key, Entry{value, std::prev(_list.end()), weight});
        }
        catch (...)
        {
            _list.pop_back();
            throw;
        }
        _weight += weight;
    }

    // 命中时把条目挪到最近使用端（真正的 LRU 淘汰），返回指向缓存值的指针而不复制。
    // 候选列表一份就是几百个 WordItem，每键的热路径上应当用它而不是 get()。
    // 指针在下一次 insert / remove / clear 之前有效。
    const Value *find(const Key &key)
    {
        auto it = _map.find(key);
        if (it == _map.end())
        {
            return nullptr;
        }
        _list.splice(_list.end(), _list, it->second.position);
        return &it->second.value;
    }

    std::optional<Value> get(const Key &key) const
    {
        auto it = _map.find(key);
        if (it != _map.end())
        {
            return it->second.value;
        }
        return std::nullopt;
    }

    bool remove(const Key &key)
    {
        auto it = _map.find(key);
        if (it == _map.end())
            return false;

        _weight -= it->second.weight;
        _list.erase(it->second.position);
        _map.erase(it);
        return true;
    }

    void clear()
    {
        _map.clear();
        _list.clear();
        _weight = 0;
    }

    size_t size() const
    {
        return _map.size();
    }

    bool empty() const
    {
        return _map.empty();
    }

    size_t weight() const
    {
        return _weight;
    }

  private:
    struct Entry
    {
        Value value;
        typename std::list<Key>::iterator position;
        size_t weight;
    };
    size_t _capacity;
    size_t _max_weight;
    size_t _weight = 0;
    std::list<Key> _list;
    std::unordered_map<Key, Entry> _map;
};
