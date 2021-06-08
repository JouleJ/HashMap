#include <algorithm>
#include <forward_list>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

template<class Iter>
size_t my_distance(Iter begin, Iter end) {
    size_t dist = 0;
    while (begin != end) {
        ++begin;
        ++dist;
    }
    return dist;
}

template<class Key, class Value, class Hash = std::hash<Key>>
class HashMap {
private:
    using Bucket = std::forward_list<std::pair<Key, Value>>;
    using BucketIter = typename Bucket::iterator;
    using BucketVectorIter = typename std::vector<Bucket>::iterator;
    Hash get_hash;
    std::vector<Bucket> buckets;
    size_t size_ = 0;

    void set_buckets_number(size_t n) {
        std::vector<Bucket> new_buckets(n);
        for (const auto& bucket: buckets) {
            for (const auto& elem: bucket) {
                new_buckets[get_hash(elem.first) % n].push_front(elem);
            }
        }
        buckets = std::move(new_buckets);
    }

    size_t guess_buckets_num_from_size(size_t size) const {
        return 4 * size + 1;
    }

    bool needs_to_update(size_t n, size_t m) const {
        return (n > 2 * m || m > 2 * n);
    }

    bool update_buckets_num() {
        const size_t n = guess_buckets_num_from_size(size_);
        if (needs_to_update(n, buckets.size())) {
            set_buckets_number(n);
            return true;
        }
        return false;
    }

    void insert_key_value_pair(const Key& new_key, const Value& new_value) {
        size_t i = get_hash(new_key) % buckets.size();
        for (const auto& elem: buckets[i]) {
            if (elem.first == new_key) {
                return;
            }
        }
        buckets[i].emplace_front(new_key, new_value);
        ++size_;
    }

    void erase_key(const Key& key) {
        size_t i = get_hash(key) % buckets.size();
        const auto bucket_begin = buckets[i].begin();
        const auto bucket_end = buckets[i].end();
        auto prev_iter = bucket_end;
        for (auto iter = bucket_begin; iter != bucket_end; ++iter) {
            if (iter->first == key) {
                if (prev_iter == bucket_end) {
                    buckets[i].pop_front();
                } else {
                    buckets[i].erase_after(prev_iter);
                }
                --size_;
                break;
            }
            prev_iter = iter;
        }
    }

public:
    class Iterator {
        HashMap* master;
        BucketVectorIter bucket_vec_iter;
        BucketIter bucket_iter;

        void skip_empty() {
            while (bucket_vec_iter != master->buckets.end() && bucket_iter == bucket_vec_iter->end()) {
                ++bucket_vec_iter;
                if (bucket_vec_iter == master->buckets.end()) {
                    bucket_iter = master->buckets.front().begin();
                } else {
                    bucket_iter = bucket_vec_iter->begin();
                }
            }
        }

        void increase() {
            ++bucket_iter;
            skip_empty();
        }

        Iterator(HashMap* _master,
                 BucketVectorIter _bucket_vec_iter,
                 BucketIter _bucket_iter):
                    master(_master),
                    bucket_vec_iter(_bucket_vec_iter),
                    bucket_iter(_bucket_iter) {
            skip_empty();
        }

        bool equals(const Iterator& other) const {
            return std::tie(bucket_vec_iter, bucket_iter) == std::tie(other.bucket_vec_iter, other.bucket_iter);
        }

        std::pair<const Key, Value>* get_ptr() {
            std::pair<Key, Value>* ptr = &(*bucket_iter);
            return (std::pair<const Key, Value>*)(void*)ptr;
        }

    public:
        friend HashMap;
        Iterator() {}

        Iterator& operator++() {
            increase();
            return *this;
        }

        Iterator operator++(int) {
            const auto clone = *this;
            increase();
            return clone;
        }

        bool operator==(const Iterator& other) const {
            return equals(other);
        }

        bool operator!=(const Iterator& other) const {
            return !equals(other);
        }

        std::pair<const Key, Value>& operator*() {
            return *get_ptr();
        }

        std::pair<const Key, Value>* operator->() {
            return get_ptr();
        }
    };

    class ConstIterator {
        Iterator base;

        ConstIterator(const Iterator& _base): base(_base) {}

    public:
        friend HashMap;
        ConstIterator() {}

        ConstIterator& operator++() {
            base.increase();
            return *this;
        }

        ConstIterator operator++(int) {
            const auto clone = *this;
            base.increase();
            return clone;
        }

        bool operator==(const ConstIterator& other) const {
            return base.equals(other.base);
        }

        bool operator!=(const ConstIterator& other) const {
            return !base.equals(other.base);
        }

        const std::pair<const Key, Value>& operator*() {
            return *base;
        }

        const std::pair<const Key, Value>* operator->() {
            return base.operator->();
        }
    };

    HashMap(const Hash& hasher = Hash()): get_hash(hasher) {
        const size_t n = guess_buckets_num_from_size(size_);
        set_buckets_number(n);
    }

    template<class Iter>
    HashMap(Iter begin,
            Iter end,
            const Hash& hasher = Hash()
    ): get_hash(hasher) {
        const size_t count = my_distance(begin, end);
        const size_t n = guess_buckets_num_from_size(count);
        set_buckets_number(n);
        for (Iter it = begin; it != end; ++it) {
            insert_key_value_pair(it->first, it->second);
        }
    }

    HashMap(const std::initializer_list<std::pair<Key, Value>>& list,
            const Hash& hasher = Hash()
    ): HashMap(list.begin(), list.end(), hasher) {}

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size() == 0;
    }

    Hash hash_function() const {
        return get_hash;
    }

    void insert(const std::pair<Key, Value>& elem) {
        insert_key_value_pair(elem.first, elem.second);
        update_buckets_num();
    }

    void erase(const Key& key) {
        erase_key(key);
        update_buckets_num();
    }

    Iterator begin() {
        Iterator iter(this, buckets.begin(), buckets.front().begin());
        return iter;
    }

    Iterator end() {
        Iterator iter(this, buckets.end(), buckets.front().begin());
        return iter;
    }

    ConstIterator begin() const {
        HashMap* mutable_this = (HashMap*)(void*)this;
        return ConstIterator(mutable_this->begin());
    }

    ConstIterator end() const {
        HashMap* mutable_this = (HashMap*)(void*)this;
        return ConstIterator(mutable_this->end());
    }

    Iterator find(const Key& key) {
        size_t i = get_hash(key) % buckets.size();
        const auto buckets_iter = buckets.begin() + i;
        for (auto iter = buckets_iter->begin(); iter != buckets_iter->end(); ++iter) {
            if (iter->first == key) {
                return Iterator(this, buckets_iter, iter);
            }
        }
        return end();
    }

    const ConstIterator find(const Key& key) const {
        HashMap* mutable_this = (HashMap*)(void*)this;
        return ConstIterator(mutable_this->find(key));
    }

    Value& operator[](const Key& key) {
        size_t i = get_hash(key) % buckets.size();
        for (auto& key_value: buckets[i]) {
            if (key_value.first == key) {
                return key_value.second;
            }
        }
        buckets[i].emplace_front(key, Value());
        ++size_;
        if(update_buckets_num()) {
            return operator[](key);
        } else {
            return buckets[i].front().second;
        }
    }

    const Value& at(const Key& key) const {
        size_t i = get_hash(key) % buckets.size();
        for (auto& key_value: buckets[i]) {
            if (key_value.first == key) {
                return key_value.second;
            }
        }
        throw std::out_of_range("HashMap::at()");
    }

    void clear() {
        buckets.clear();
        size_ = 0;
        set_buckets_number(guess_buckets_num_from_size(size_));
    }
};