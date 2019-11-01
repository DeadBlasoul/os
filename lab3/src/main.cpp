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
static DWORD thread_launcher(LPVOID data) {
    static_assert(std::is_invocable_v<callable>, "can not start from not callable");

    callable* callback = static_cast<callable*>(data);
    std::invoke(*callback);

    return EXIT_SUCCESS;
}

//! cpus count
/** Returns cpus count. If `required` is 0 function returns number of available cpus.
    If `required` is greater than available cpus the maximum will be returned. */
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

static void read_input(std::istream& in, std::vector<std::vector<bool>>& links) {
    using std::vector;
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

    links = vector(number_of_vertices, vector(number_of_vertices, false));
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

static int run_parallel_bfs(std::vector<std::vector<bool>>& links, size_t threads_count) {
    using std::pair;
    using std::tuple;
    using std::vector;
    using tbb::concurrent_bounded_queue;
    using tbb::mutex;
    using tbb::atomic;
    using std::cerr;
    using std::endl;
    using std::terminate;

    typedef size_t index;
    typedef size_t from_index;

    enum class task_type {
        STOP,
        CONTINUE,
    };

    enum class report_type {
        REPORT,
        SEARCH_IS_DONE
    };

    using task   = tuple<task_type, index, from_index>;
    using report = pair<report_type, bool>;

    index constexpr start           = 0;
    auto constexpr  cycle_found     = true;
    auto constexpr  cycle_not_found = false;

    //! Synchronization resources
    concurrent_bounded_queue<task>   tasks;
    concurrent_bounded_queue<report> reports;
    vector<bool>                     map(links.size(), false);
    mutex                            map_lock;
    atomic<bool>                     search_done = false;
    atomic<size_t>                   threads_blocked = 0;

    //! Thread routine.
    /** Main procedure that is running in each search thread. */
    auto routine = [&]() -> void {
        task t;
        while (!search_done) {
            //! blocked is used to check, how many threads now are blocked
            /** If count of blocked threads is `threads_count - 1` then we are nearly
                to complete workers block. To avoid it we send report that all threads are done
                and waiting for a STOP task. */
            size_t blocked = threads_blocked.fetch_and_increment();
            if (blocked + 1 == threads_count) {
                reports.emplace(report_type::SEARCH_IS_DONE, false);
            }
            tasks.pop(t);
            threads_blocked.fetch_and_decrement();

            auto [type, current, from] = t;
            if (type == task_type::STOP) {
                search_done = true;
                continue;
            }

            // Update map
            // Scope created for RAII
            {
                std::lock_guard<decltype(map_lock)> lock(map_lock);

                //! been_here is a reference
                /** The type of been_here is a special wrapper because we used std::vector<bool>
                    that has well memory optimization.
                    But in context of algorithm this variable is a boolean that we use to check
                    where we have been before. */
                auto been_before = map[current];
                if (been_before == false) {
                    been_before = true;
                } else {
                    search_done = true;
                    reports.emplace(report_type::REPORT, cycle_found);
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
            reports.emplace(report_type::REPORT, cycle_not_found);
        }
    };

    //! Initial state
    /** Tell first thread to start from vertex with index that equals start. */
    tasks.emplace(task_type::CONTINUE, start, start);

    // Initialize threads
    vector<HANDLE> threads(threads_count, HANDLE{ NULL });
    for (auto& thread : threads) {
        thread = ::CreateThread(
            NULL,
            0,
            thread_launcher<decltype(routine)>,
            static_cast<LPVOID>(&routine),
            NULL,
            NULL
        );
        if (thread == NULL) {
            // Fatal error, abort
            cerr << "thread initialization failed, aborting..." << endl;
            terminate();
        }
    }

    //! Gathering reports
    bool answer = false;
    bool reports_done = false;
    while (!reports_done) {
        report result;
        reports.pop(result);

        switch (result.first) {
        case report_type::REPORT:
            // Update information about cycles in the graph
            answer |= result.second;
            if (!answer) {
                // Cycle is still not found, continue operations
                continue;
            }
            [[ fallthrough ]];
        case report_type::SEARCH_IS_DONE:
            // Search is done, break loop
            break;
        }

        reports_done = true;
    }

    // Abort all threads
    for (size_t i = 0; i < threads_count; i++) {
        tasks.emplace(task_type::STOP, 0, 0);
    }

    // Wait for threads
    ::WaitForMultipleObjects(DWORD(threads.size()), threads.data(), TRUE, INFINITE);

    return answer;
}

int main(int argc, char* argv[]) try {
    using std::vector;
    using std::cin;
    using std::cout;
    using std::cerr;
    using std::endl;

    // Get cpus count
    size_t cpus = 0;
    parse(argc, argv, &cpus);
    cpus = cpus_count(cpus);

    // Get input data
    vector<vector<bool>> links;
    read_input(cin, links);

    // Run parallel search of cycles in graph
    bool result = run_parallel_bfs(links, cpus);
    cout << "cycle exists: " << (result ? "true" : "false") << endl;

    return EXIT_SUCCESS;
}
catch (std::exception& e) {
    using std::cerr;
    using std::endl;

    cerr << "error: " << e.what() << endl;
}
