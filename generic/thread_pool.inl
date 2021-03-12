// Thread pool with better support for async / future style operations.
// Example usage:
//
// parallel_for(start, end, [&](i){ ... });
// parallel_invoke(start, end, [&](i){ ... });
//
// idx = parallel_for_future(start, end, [&](i){ ... });
// idx = parallel_invoke_future(start, end, [&](i){ ... });
//
// idx = enqueueTask(f);
// idxRange = enqueueTasks(f1, f2, f3...);
// idx = enqueueTasksAsGroup(f1, f2, f3...);
//
// runNextTask();
// runTasksUntil(predicate);
// waitForTask(idx);
// waitForTasks(idxRange);
// waitForAllTasks();
//
// setThreadCount(n); // Must be called before doing things.
//


/////////////////////////////////////////////////////
// .h
////////////////////////////////////////////////////

#include <atomic>
#include <array>
#include <ctime>
#include <mutex>
#include <functional>
#include <shared_mutex>
#include <memory>
#include <thread>
#include <vector>
#include <set>
#include <utility>
#include <condition_variable>


using taskhandle_t = size_t;
const static taskhandle_t INVALID_TASK_HANDLE = ~taskhandle_t(0);


class TaskPool {
public:
    struct Task {
        virtual ~Task() = default;
        virtual void execute(void) = 0;
    };

private:
    using shared_mutex = std::shared_timed_mutex;

    // Task which upon completion, notifies the taskpool
    template<typename F>
    struct TaskWithClosure : Task {

    TaskWithClosure(F func, TaskPool* parentPool, const taskhandle_t taskId)
    : m_func(std::move(func)), m_parentPool(parentPool), m_taskId(taskId)
    {}

    void execute(void) final
    {
        m_func();
        m_parentPool->taskClosure(m_taskId);
    }

    private:
        F m_func;
        TaskPool* m_parentPool;
        taskhandle_t  m_taskId;

    };

    void taskClosure(const taskhandle_t taskId);

public:
    // Attempt to run the next task, if there was no task, this returns false.
    bool runNextTask();

    // Check up on the status of tasks etc
    bool hasTaskFinished(const taskhandle_t tid) const;
    bool hasTasks() const { return !m_tasks.empty(); }
    bool allTasksFinished() const { return m_unrunIds.empty(); }


    // Ensure tasks
    template<typename F>
    taskhandle_t enqueueTask(F&& f)
    {
        const taskhandle_t id = reserveTaskId();

        {
            std::unique_lock<shared_mutex> guard(m_unrunIdsLock);
            m_unrunIds.insert(id);
        }

        std::unique_ptr<Task> task = allocateTask(std::forward<F>(f), id);
        {
            std::lock_guard<std::mutex> guard(m_tasksLock);
            m_tasks.push_back(std::move(task));
        }
        return id;
    }

    // Returns back a range of ids
    template<typename... Fs>
    std::pair<taskhandle_t, taskhandle_t> enqueueTasks(Fs&&... fs)
    {
        std::pair<taskhandle_t, taskhandle_t> idRange = reserveTaskIdRange(sizeof...(Fs));

        {
            std::unique_lock<shared_mutex> guard(m_unrunIdsLock);
            for(taskhandle_t id=idRange.first; id<idRange.second; ++id)
            {
                m_unrunIds.insert(id);
            }
        }

        auto tasks = allocateTasks(idRange.first, std::forward<Fs>(fs)...);
        {
            std::lock_guard<std::mutex> guard(m_tasksLock);
            for(std::unique_ptr<Task>& task : tasks)
            {
                m_tasks.push_back(std::move(task));
            }
        }
        return idRange;
    }

private:
    // Reserve task ids
    taskhandle_t reserveTaskId() { return m_taskIdIota++; }
    std::pair<taskhandle_t, taskhandle_t> reserveTaskIdRange(const size_t count)
    {
        taskhandle_t lastId = m_taskIdIota += count;
        return std::make_pair(lastId-count, lastId);
    }

    // Allocate tasks
    template<typename F>
    std::unique_ptr<Task> allocateTask(F&& f, const taskhandle_t taskId)
    {
        return std::make_unique<TaskWithClosure<F>>(
            std::forward<F>(f),
            this,
            taskId
        );
    }

    template<typename... Fs>
    std::array<std::unique_ptr<Task>, sizeof...(Fs)> allocateTasks(const taskhandle_t startingTaskId, Fs&&... fs)
    {
        taskhandle_t currentId = startingTaskId;
        return {
            allocateTask(std::forward<Fs>(fs), currentId++)...
        };
    }

    std::atomic<taskhandle_t>          m_taskIdIota {0};
    std::mutex                         m_tasksLock;
    std::vector<std::unique_ptr<Task>> m_tasks;

    mutable shared_mutex               m_unrunIdsLock;
    std::set<taskhandle_t>             m_unrunIds;

};


class ThreadedTaskPool
{

public:
    ~ThreadedTaskPool();

    template<typename... Fs>
    void parallel_invoke(Fs&&... fs)
    {
        waitForTasks(enqueueTasks(std::forward<Fs>(fs)...));
    }

    template<typename It, typename F>
    void parallel_for(It begin, It end, F&& f)
    {
        int count = end - begin;
        if(count <= 1)
        {
            switch(count)
            {
                case -1:
                case 1: f(begin);
                case 0: return;
                default:
                    std::swap(begin, end);
                    count = -count;
                    break;
            }
        }

        const int maxThreadCount = (int)m_maxThreadCount;
        std::atomic<It> it { begin };
        std::atomic<int> subTaskCount { 0 };

        // So we don't nessacarily want to just spawn a bunch of tasks
        // especially if #1 the pool is already busy and won't pick them
        // up, and #2 if the actual work itself is really cheap.
        // Rather, we enqueue a task, that itself enqueues another task
        // and so on and so forth until either the threads are busy
        // or until the work is done on whatever threads they are on.

        // Recursive lambda shenanigans:
        // http://pedromelendez.com/blog/2015/07/16/recursive-lambdas-in-c14/
        auto processTasks = [&]() {
            auto worker = [](
                    ThreadedTaskPool* tp,
                    F&& func,
                    int maxThreadCount,
                    std::atomic<It>& it,
                    std::atomic<int>& subTaskCount,
                    const It end,
                    auto self
            ) -> void
            {
                taskhandle_t childHandle = INVALID_TASK_HANDLE;
                // Spawn a new task
                if(subTaskCount < std::min(int(end - it), maxThreadCount))
                {
                    ++subTaskCount;
                    childHandle = tp->enqueueTask([&]{
                        self(
                            tp,
                            std::forward<F>(func),
                            maxThreadCount,
                            it,
                            subTaskCount,
                            end,
                            self
                        );
                    });
                }

                for(It c=it++; c<end; c=it++)
                {
                    func(c);
                }

                if(childHandle != INVALID_TASK_HANDLE)
                {
                    tp->waitForTask(childHandle);
                }
            };
            worker(
                this,
                std::forward<F>(f),
                maxThreadCount,
                it,
                subTaskCount,
                end,
                worker
            );
        };

        ++subTaskCount;
        waitForTask(enqueueTask([&]{processTasks();}));
    }

    template<typename... Fs>
    taskhandle_t parallel_invoke_future(Fs&&... fs)
    {
        return enqueueTasksAsGroup(std::forward<Fs>(fs)...);
    }

    template<typename It, typename F>
    taskhandle_t parallel_for_future(It begin, It end, F&& f)
    {
        ThreadedTaskPool* self = this;
        return enqueueTask([self, begin, end, f=std::move(f)]{
            self->parallel_for(begin, end, std::move(f));
        });
    }

    bool runNextTask();
    void waitForTask(const taskhandle_t handle);
    void waitForTasks(const std::pair<taskhandle_t, taskhandle_t> handleRange);
    void waitForAllTasks()
    {
        runTasksUntil([&]{
            return m_taskPool.allTasksFinished();
        });
    }

    template<typename F>
    taskhandle_t enqueueTask(F&& f)
    {
        if(!m_launchedThreads){ spinUpThreads(); }
        taskhandle_t hnd = m_taskPool.enqueueTask(std::forward<F>(f));
        newTaskAdded();
        return hnd;
    }

    template<typename... Fs>
    std::pair<taskhandle_t, taskhandle_t> enqueueTasks(Fs&&... fs)
    {
        if(!m_launchedThreads){ spinUpThreads(); }
        std::pair<taskhandle_t, taskhandle_t> hnds = m_taskPool.enqueueTasks(
            std::forward<Fs>(fs)...
        );
        newTasksAdded();
        return hnds;
    }

    template<typename... Fs>
    taskhandle_t enqueueTasksAsGroup(Fs&&... fs)
    {
        ThreadedTaskPool* self = this;
        std::pair<taskhandle_t, taskhandle_t> taskRange = enqueueTasks(
            std::forward<Fs>(fs)...
        );
        return enqueueTask([self, taskRange]{
            self->waitForTasks(taskRange);
        });
    }

    template<typename Predicate>
    void runTasksUntil(Predicate&& pred)
    {
        while(!pred())
        {
            // If there are no more tasks left, enter a wait state
            if(!runNextTask())
            {
            // since we assume that
                m_taskFinishedWaiter.wait([&]{
                    return m_taskPool.hasTasks() || pred();
                });
            }
        }
    }

    void setThreadCount(const uint32_t threadCount)
    {
        if(m_launchedThreads)
        {
            // Can't set thread count after threads have started work
            // (unless I really need to, then I suppose we can do a tear down etc)
            std::abort();
        }
        if(threadCount > 0)
        {
            m_maxThreadCount = threadCount - 1;
        }
        else
        {
            m_maxThreadCount = 0;
        }
    }

private:
    void workerRoutine(void);
    void spinUpThreads(void);

    void taskFinished(void) const { m_taskFinishedWaiter.wakeAll(); }
    void newTaskAdded(void) const { m_newTaskWaiter.wakeOne(); }
    void newTasksAdded(void) const { m_newTaskWaiter.wakeAll(); }

    struct TaskWaiter
    {
        template<typename Predicate>
        void wait(Predicate&& readyPredicate)
        {
            if(!readyPredicate())
            {
                ++m_waitCount;
                std::unique_lock<std::mutex> lock(m_lock);
                m_cv.wait(lock, std::forward<Predicate>(readyPredicate));
                --m_waitCount;
            }
        }

        void wait()
        {
            ++m_waitCount;
            std::unique_lock<std::mutex> lock(m_lock);
            m_cv.wait(lock);
            --m_waitCount; 
        }

        void wakeOne(void) { if(m_waitCount > 0) { m_cv.notify_one(); } }
        void wakeAll(void) { if(m_waitCount > 0) { m_cv.notify_all(); } }

        std::atomic<uint32_t>   m_waitCount { 0 };
        std::mutex              m_lock;
        std::condition_variable m_cv;
    };


    TaskPool                 m_taskPool;

    mutable TaskWaiter       m_taskFinishedWaiter;
    mutable TaskWaiter       m_newTaskWaiter;

    uint32_t                 m_maxThreadCount = std::thread::hardware_concurrency() - 1;
    uint8_t                  m_launchedThreads = false;
    uint8_t                  m_stopWorking = false; // kill switch

    std::vector<std::thread> m_threads;
    std::mutex               m_threadLaunchLock;

};



/////////////////////////////////////////////////////
// .cpp
////////////////////////////////////////////////////

ThreadedTaskPool GLOBAL_THREAD_POOL;


bool TaskPool::runNextTask()
{
    std::unique_ptr<Task> task;

    if(!hasTasks()) { return false; }

    {
        std::lock_guard<std::mutex> guard(m_tasksLock);
        if(!hasTasks()) { return false; }
        task = std::move(m_tasks.back());
        m_tasks.pop_back();
    }

    task->execute();
    return true;
}

bool TaskPool::hasTaskFinished(const taskhandle_t taskId) const
{
    if(taskId == INVALID_TASK_HANDLE) { return true; }
    std::shared_lock<shared_mutex> guard(m_unrunIdsLock);
    return m_unrunIds.find(taskId) == m_unrunIds.end();
}

void TaskPool::taskClosure(const taskhandle_t taskId) {
    std::unique_lock<shared_mutex> guard(m_unrunIdsLock);
    m_unrunIds.erase(taskId);
}


ThreadedTaskPool::~ThreadedTaskPool()
{
    waitForAllTasks();
    m_stopWorking = true;
    m_taskFinishedWaiter.wakeAll();
    m_newTaskWaiter.wakeAll();
    for(std::thread& t : m_threads)
    {
        t.join();
    }
}

bool ThreadedTaskPool::runNextTask()
{
    if(m_taskPool.runNextTask())
    {
        taskFinished();
        return true;
    }
    return false;
}

void ThreadedTaskPool::waitForTask(const taskhandle_t handle)
{
    runTasksUntil([&]{ return m_taskPool.hasTaskFinished(handle); });
}

void ThreadedTaskPool::waitForTasks(const std::pair<taskhandle_t, taskhandle_t> handleRange)
{
    for(taskhandle_t currentId=handleRange.first; currentId<handleRange.second; ++currentId)
    {
        waitForTask(currentId);
    }
}

void ThreadedTaskPool::workerRoutine()
{
    while(!m_stopWorking)
    {
        // If there are no more tasks left, enter a wait state
        if(!runNextTask())
        {
            m_newTaskWaiter.wait();
        }
    }
}

void ThreadedTaskPool::spinUpThreads()
{
    if(!m_launchedThreads)
    {
        std::lock_guard<std::mutex> guard(m_threadLaunchLock);
        if(!m_launchedThreads)
        {
            for(uint32_t i=0; i < m_maxThreadCount ; ++i)
            {
                m_threads.emplace_back(&ThreadedTaskPool::workerRoutine, this);
            }
            m_launchedThreads = true;
        }
    }
}


/////////////////////////////////////////////////////
// global .h
////////////////////////////////////////////////////

extern ThreadedTaskPool GLOBAL_THREAD_POOL;


template<typename... Fs>
inline void parallel_invoke(Fs&&... fs)
{
    GLOBAL_THREAD_POOL.parallel_invoke(std::forward<Fs>(fs)...);
}

template<typename It, typename F>
inline void parallel_for(It begin, It end, F&& f)
{
    GLOBAL_THREAD_POOL.parallel_for(begin, end, std::forward<F>(f));
}

template<typename... Fs>
inline taskhandle_t parallel_invoke_future(Fs&&... fs)
{
    return GLOBAL_THREAD_POOL.parallel_invoke_future(std::forward<Fs>(fs)...);
}

template<typename It, typename F>
inline taskhandle_t parallel_for_future(It begin, It end, F&& f)
{
    return GLOBAL_THREAD_POOL.parallel_for_future(begin, end, std::forward<F>(f));
}

inline bool runNextTask()
{
    return GLOBAL_THREAD_POOL.runNextTask();
}

inline void waitForTask(const taskhandle_t handle)
{
    GLOBAL_THREAD_POOL.waitForTask(handle);
}

inline void waitForTasks(const std::pair<taskhandle_t, taskhandle_t> handleRange)
{
    GLOBAL_THREAD_POOL.waitForTasks(handleRange);
}

inline void waitForAllTasks()
{
    GLOBAL_THREAD_POOL.waitForAllTasks();
}

inline void setThreadCount(const uint32_t threadCount)
{
    GLOBAL_THREAD_POOL.setThreadCount(threadCount);
}

template<typename F>
inline taskhandle_t enqueueTask(F&& f)
{
    return GLOBAL_THREAD_POOL.enqueueTask(std::forward<F>(f));
}

template<typename... Fs>
inline std::pair<taskhandle_t, taskhandle_t> enqueueTasks(Fs&&... fs)
{
    return GLOBAL_THREAD_POOL.enqueueTasks(std::forward<Fs>(fs)...);
}

template<typename... Fs>
inline taskhandle_t enqueueTasksAsGroup(Fs&&... fs)
{
    return GLOBAL_THREAD_POOL.enqueueTasksAsGroup(std::forward<Fs>(fs)...);
}

template<typename Predicate>
inline void runTasksUntil(Predicate&& pred)
{
    GLOBAL_THREAD_POOL.runTasksUntil(std::forward<Predicate>(pred));
}

