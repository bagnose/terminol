// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/cache.hxx"

#include <vector>

int main() {
    Cache<int, std::string> cache(3);
    std::vector<int> keys;

    keys.clear();
    for (auto & pair : cache) { keys.push_back(pair.first); }
    ENFORCE(keys.empty(), "");
    //ENFORCE(cache.lookup(11) == nullptr, "");
    ENFORCE(cache.find(11) == cache.end(), "");

    // Add 6
    cache.insert(6, "Degrees of Kevin Bacon.");
    //ENFORCE(cache.lookup(6) != nullptr, "");
    ENFORCE(cache.find(6) != cache.end(), "");
    //ENFORCE(cache.lookup(-1) == nullptr, "");
    ENFORCE(cache.find(-1) == cache.end(), "");
    keys.clear();
    for (auto & pair : cache) { keys.push_back(pair.first); }
    ENFORCE(keys.size() == 1, "");
    ENFORCE(keys[0] == 6, "");

    // Add 42
    cache.insert(42,  "The meaning of life.");
    keys.clear();
    for (auto & pair : cache) { keys.push_back(pair.first); }
    ENFORCE(keys.size() == 2, "");
    ENFORCE(keys[0] == 42, "");
    ENFORCE(keys[1] == 6, "");

    // Lookup 42, shouldn't change the cache order
    keys.clear();
    for (auto & pair : cache) { keys.push_back(pair.first); }
    ENFORCE(keys.size() == 2, "");
    ENFORCE(keys[0] == 42, "");
    ENFORCE(keys[1] == 6, "");

    // Lookup 6, should reverse the cache order
    cache.find(6);
    keys.clear();
    for (auto & pair : cache) { keys.push_back(pair.first); }
    ENFORCE(keys.size() == 2, "");
    ENFORCE(keys[0] == 6, "");
    ENFORCE(keys[1] == 42, "");

    // Lookup 6, shouldn't change the cache order
    cache.find(6);
    keys.clear();
    for (auto & pair : cache) { keys.push_back(pair.first); }
    ENFORCE(keys.size() == 2, "");
    ENFORCE(keys[0] == 6, "");
    ENFORCE(keys[1] == 42, "");

    // Add 99 and 666, this should push 42 out
    cache.insert(99,  "Luftballons.");
    cache.insert(666, "The number of the beast.");
    keys.clear();
    for (auto & pair : cache) { keys.push_back(pair.first); }
    ENFORCE(keys.size() == 3, "");
    ENFORCE(keys[0] == 666, "");
    ENFORCE(keys[1] == 99, "");
    ENFORCE(keys[2] == 6, "");

    return 0;
}
