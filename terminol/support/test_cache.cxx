// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/cache.hxx"
#include "terminol/support/test.hxx"

#include <vector>

template <typename Key, typename T>
void enforceKeys(Cache<Key, T> & cache, std::initializer_list<Key> keys) {
    auto iter1 = cache.begin();
    auto iter2 = keys.begin();

    while (iter1 != cache.end() && iter2 != keys.end()) {
        // std::cout << iter1->first << " " << *iter2 << std::endl;
        ENFORCE(iter1->first == *iter2, << "Key mismatch");

        ++iter1;
        ++iter2;
    }

    ENFORCE(iter1 == cache.end(), << "More keys than expected");
    ENFORCE(iter2 == keys.end(), << "Fewer keys than expected");
}

template <typename Key, typename T>
void enforceIteration(Cache<Key, T> & cache) {
    auto iter  = cache.begin();
    auto rIter = cache.rend();

    while (iter != cache.end()) {
        --rIter;
        ENFORCE(iter->first == rIter->first, << "Match");
        ++iter;
    }
}

int main() {
    Test test("support/cache");

    Cache<int, std::string> cache;

    test.enforce(cache.empty(), "empty cache");
    test.enforce(cache.find(11) == cache.end(), "");

    test.enforce(cache.find(6) == cache.end(), "");
    cache.insert(6, "degrees of kevin bacon");
    test.enforce(cache.find(6) != cache.end(), "");
    test.enforce(cache.at(6) == "degrees of kevin bacon", "");
    enforceKeys(cache, {6});

    cache.insert(42, "the meaning of life");
    enforceKeys(cache, {6, 42});

    cache.insert(99, "luft balloons");
    enforceKeys(cache, {6, 42, 99});
    enforceIteration(cache);

    cache.find(6);
    enforceKeys(cache, {42, 99, 6});

    cache.find(99);
    enforceKeys(cache, {42, 6, 99});

    cache.erase(cache.find(6));
    enforceKeys(cache, {42, 99});

    return 0;
}
