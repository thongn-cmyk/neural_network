#include <memory>

struct Foo{};

int main()
{
    std::shared_ptr<Foo> foo        = {};

    std::shared_ptr<int> bar        = std::static_pointer_cast<int>(std::static_pointer_cast<void>(foo));
    std::shared_ptr<Foo> foo_2      = std::static_pointer_cast<Foo>(std::static_pointer_cast<void>(bar));
    std::shared_ptr<int[]> bar_2    = std::static_pointer_cast<int[]>(std::static_pointer_cast<int>(bar));
    std::shared_ptr<int[]> arr      = std::make_unique<int[]>(1);
}