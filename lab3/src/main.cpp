/* stdlib: */
#include <cstdlib>
#include <vector>
#include <sstream>
#include <iostream>
#include <type_traits>
#include <utility>
#include <tuple>
#include <mutex>
#include <thread>

/* WINAPI: */
#include <Windows.h>
#include <processthreadsapi.h>

/* Threading building blocks: */
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_vector.h>
#include <tbb/mutex.h>
#include <tbb/atomic.h>

template<typename callable>
DWORD thread_launcher(LPVOID data) {
    static_assert(std::is_invocable_v<callable>, "can not start from not callable");

    callable* callback = static_cast<callable*>(data);
    return std::invoke(*callback);
}

/*
    cpus_count

    Returns cpus count 

    If `required` is 0 function returns number of available cpus
*/
static size_t cpus_count(size_t required = 0) noexcept {
    size_t possible;
    SYSTEM_INFO sysinfo;

    ::GetSystemInfo(&sysinfo);
    if (required == 0) {
        possible = size_t(sysinfo.dwNumberOfProcessors);
    }
    else {
        possible = min(required, size_t(sysinfo.dwNumberOfProcessors));
    }

    return possible;
}

static void parse(int argc, char* argv[], size_t* cpus_count) {
    using std::stringstream;
    using std::runtime_error;

    if (argc > 2) {
        throw runtime_error("too many arguments");
    }
    if (argc == 1) {
        return;
    }

    size_t cpus;
    auto cpus_count_str = argv[1];
    stringstream stream(cpus_count_str);
    stream >> cpus;
    if (stream.fail()) {
        throw runtime_error("incorrect arguments");
    }

    *cpus_count = cpus;
}

static void read_input(std::istream& in, tbb::concurrent_vector<tbb::concurrent_vector<bool>>& links) {
    using tbb::concurrent_vector;
    using std::runtime_error;

    auto constexpr minimum_number_of_vertices = 2;
    auto constexpr maximum_number_of_vertices = 1000;

    size_t number_of_vertices;
    in >> number_of_vertices;
    if (in.fail()) {
        throw runtime_error("incorrect input");
    }
    if (number_of_vertices < minimum_number_of_vertices) {
        throw runtime_error("number of vertices is too low");
    }
    if (number_of_vertices > maximum_number_of_vertices) {
        throw runtime_error("number of vertices is too high");
    }

    links = concurrent_vector(number_of_vertices, concurrent_vector(number_of_vertices, false));
    while (in) {
        size_t u, v;
        in >> u >> v;
        if (in.fail()) {
            if (!in.eof()) {
                throw runtime_error("incorrect input");
            }
            continue;
        }

        links[u][v] = true;
        links[v][u] = true;
    }
}

static int run_parallel_bfs(tbb::concurrent_vector<tbb::concurrent_vector<bool>>& links, size_t cpus_count, bool& result) {
    using std::pair;
    using std::tuple;
    using std::vector;
    using tbb::concurrent_bounded_queue;
    using tbb::concurrent_vector;
    using tbb::mutex;
    using tbb::atomic;

    typedef size_t index;
    typedef size_t from_index;
    enum class task_type {
        STOP,
        CONTINUE,
    };

    using task   = tuple<task_type, index, from_index>;

    index constexpr start           = 0;
    auto constexpr  cycle_found     = true;
    auto constexpr  cycle_not_found = false;

    concurrent_bounded_queue<task> tasks;
    concurrent_bounded_queue<bool> reports;
    concurrent_vector<bool>        map(links.size(), false);
    mutex                          map_lock;
    atomic<bool>                   done = false;

    auto routine = [&]() -> DWORD {
        task t;
        while (!done) {
            tasks.pop(t);
            auto [type, current, from] = t;
            if (type == task_type::STOP) {
                done = true;
                continue;
            }

            // Update map
            // Scope created for RAII
            {
                std::lock_guard<decltype(map_lock)> lock(map_lock);
                bool& been_here = map[current];

                if (been_here == false) {
                    been_here = true;
                } else {
                    done = true;
                    reports.push(cycle_found);
                    continue;
                }
            }

            // Remove path we came from
            links[current][from] = false;

            // Push new tasks
            for (index i = 0; i < links.size(); ++i) {
                if (links[current][i]) {
                    tasks.emplace(task_type::CONTINUE, i, current);
                }
            }

            // Push information about current vertex
            reports.push(cycle_not_found);
        }

        return EXIT_SUCCESS;
    };

    // Initial state
    tasks.emplace(task_type::CONTINUE, start, start);

    // Initialize threads
    vector<HANDLE> threads(cpus_count, HANDLE{ NULL });
    for (auto& thread : threads) {
        thread = ::CreateThread(
            NULL,
            0,
            thread_launcher<decltype(routine)>,
            static_cast<LPVOID>(&routine),
            NULL,
            NULL
        );
    }

    // Gathering reports
    bool answer = false; //<-- Holds information about cycles in the graph
    size_t counter = links.size();
    while (counter > 0) {
        bool result;
        reports.pop(result);
        --counter;

        answer |= result; // Update information about cycles in the graph
        if (!result && counter > 0) {
            // Cycle still not found, continue operations
            continue;
        }

        // Job done, abort all operations
        std::this_thread::sleep_for(std::chrono::milliseconds{ 1500 });
        for (size_t i = 0; i < cpus_count; i++) {
            tasks.emplace(task_type::STOP, 0, 0);
        }
        break;
    }

    // Wait for threads complete execution
    ::WaitForMultipleObjects(DWORD(threads.size()), threads.data(), TRUE, INFINITE);

    result = answer;
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) try {
    using tbb::concurrent_vector;
    using std::cin;
    using std::cout;
    using std::cerr;
    using std::endl;

    // Get cpus count
    size_t cpus = 0;
    parse(argc, argv, &cpus);
    cpus = cpus_count(cpus);

    // Get input data
    concurrent_vector<concurrent_vector<bool>> links;
    read_input(cin, links);

    bool result;
    int err = run_parallel_bfs(links, cpus, result);
    if (err != EXIT_SUCCESS) {
        cerr << "run_parallel_bfs returned error: [" << err << "]" << endl;
    }
    else {
        cout << "cycle exists: " << (result ? "true" : "false") << endl;
    }

    return err;
}
catch (std::exception& e) {
    using std::cerr;
    using std::endl;

    cerr << "error: " << e.what() << endl;
}
