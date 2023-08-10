#  strong_ptr and decay_ptr
A safe use pattern for coalescing `std::shared_ptr` into a `std::unique_ptr`.

While any such pattern would usually make no sense, it can be made to work with a new type that holds an extra reference (`strong_ptr`), and one that observes the lifetime of extra references (`decay_ptr`).

### The pattern
1. Create a `strong_ptr` like any typical smart pointer. It cannot be copied, but it can "loan out" `std::shared_ptr`s using `strong_ptr::get_shared()`.
2. Use the "loaned" `shared_ptr`s as usual, and use the `strong_ptr` as though it were a `unique_ptr`.
3. When it's time to coalesce, move the `strong_ptr` into a `decay_ptr`.
4. Query the status with `decay_ptr::decayed()` or block the current thread until decay with `decay_ptr::wait()`, `decay_ptr::wait_for()`, or `decay_ptr::wait_until()`.

Once moved into a `decay_ptr`, the original `strong_ptr` is reset, and the `decay_ptr` is unable to loan out any new `std::shared_ptr`s. Once the `decay_ptr` has decayed, it behaves just like a `std::unique_ptr`.

If a loaned `std::shared_ptr` outlives the `strong_ptr` that owns it, the lifetime is extended until the last copy is deleted. This is accomplished by creating a `std::shared_ptr` to represent the lifetime of the strong_ptr, and storing a copy of it in the loaned `shared_ptr`'s deleter.

Thus, `strong_ptr` and `decay_ptr` ensure that allocated memory is always freed.

Here's a quick example of their use:
```c++

void print_sleeping(std::shared_ptr<int> a)
{
    printf("sleeping for: %i\n", *a);
}

void print_finished(std::shared_ptr<int>&& a)
{
    printf("finished sleeping\n");
}

void thread_func(std::shared_ptr<int> a)
{
    // the shared_ptr can be copied and moved as usual.
    print_sleeping(a);
    std::this_thread::sleep_for(std::chrono::seconds(*a)); // Some long-lived operation
    print_finished(std::move(a));
}

int main()
{
    strong_ptr<int> strong(make_strong<int>(2));
    std::thread thr(thread_func, strong.get_shared());
    decay_ptr<int> dec(std::move(strong));
    // The original strong_ptr is now reset
    dec.wait();
    // dec is now guaranteed to be decayed

    thr.join();
    // dec is guaranteed to be unique here. Its memory will be freed
    // when it goes out of scope.

    return 0;
}
```

## Implementation details
- Header-only
- The implementation is c++11 and may work with some compilers, but c++14 is recommended to avoid [LWG 2315](https://cplusplus.github.io/LWG/issue2315)
- std pointers are used throughout rather than custom implementations for the sake of simplicity.
