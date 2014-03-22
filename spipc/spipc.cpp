#include "targetver.h"
#include "spipc.h"

#include <stdio.h>
#include <tchar.h>
#include <thread>

namespace spipc { namespace testing {
class Memory_Transport: public sprot::Transport_Interface
{
    public:

        Memory_Transport();
        virtual size_t read(void* buf, size_t buf_size, size_t timeout = infinite_wait);
        virtual size_t write(const void* buf, size_t buf_size, size_t timeout = infinite_wait);

    private:

        std::vector <unsigned char> buf_;
        static const size_t buf_reserve_ = 1024 * 1024;
        std::recursive_mutex write_protector_;
        std::condition_variable data_available_;
        std::condition_variable buffer_empty_;

        void reset();
};

Memory_Transport::Memory_Transport()
{
    reset();
}

void Memory_Transport::reset()
{
    std::vector<unsigned char> empty;
    buf_.swap(empty);
    buf_.reserve(buf_reserve_);
}

size_t Memory_Transport::write(const void* buf, size_t buf_size, size_t timeout)
{
    std::lock_guard<std::recursive_mutex> lock(write_protector_);
    static std::mutex mutex;
    std::unique_lock<std::mutex> buffer_empty_lock(mutex);

    if (buf_.size() != 0)
        buffer_empty_.wait(buffer_empty_lock);

    buf_.insert(buf_.end(), (unsigned char*)buf, (unsigned char*)buf + buf_size);
    data_available_.notify_all();
    return buf_size;
}

size_t Memory_Transport::read(void* buf, size_t buf_size, size_t timeout)
{
    static std::mutex mutex;
    std::unique_lock<std::mutex> data_avail_lock(mutex);

    data_available_.wait(data_avail_lock);

    size_t read_bytes = buf_size > buf_.size() ? buf_.size() : buf_size;
    memcpy(buf, &(buf_[0]), read_bytes);
    buf_.resize(0);

    buffer_empty_.notify_all();

    return read_bytes;
}

std::recursive_mutex g_test_mutex;
std::vector<std::string> g_written_items;

void th1_mem_trans_test(Memory_Transport* trans)
{
    static unsigned counter = 0;
    srand(std::this_thread::get_id().hash());

    std::vector<std::string> written_items;

    while (counter++ < 10)
    {
        int rnd_sz = 1 + rand() % 12;
    
        std::vector<char> to_write;
        for (int i = 0; i < rnd_sz; ++i)
            to_write.push_back(65 + rand() % 20);
        to_write.push_back(0);

        char* write_buf = &(*to_write.begin());
        printf("Writing: %s (%d bytes)\n", write_buf, rnd_sz + 1);

        trans->write(write_buf, rnd_sz + 1);
        written_items.push_back(write_buf);

        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_written_items.insert(g_written_items.end(), written_items.begin(), written_items.end());
}

std::vector<std::string> g_read_items;

void th2_mem_trans_test(Memory_Transport* trans)
{
    static unsigned counter = 0;
    srand(std::this_thread::get_id().hash());
    
    std::vector<std::string> read_items;

    while (counter++ < 10)
    {
        char read_buf[256];
        memset(read_buf, 0, sizeof(read_buf));
        trans->read(read_buf, sizeof(read_buf));

        read_items.push_back(read_buf);
        printf("Reading: %s\n", read_buf);

        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_read_items.insert(g_read_items.end(), read_items.begin(), read_items.end());
}

bool N_threads_mem_transport_test()
{
    Memory_Transport trans;

    std::thread writer(th1_mem_trans_test, &trans);
    std::thread reader(th2_mem_trans_test, &trans);

    writer.join();
    reader.join();

    if (g_read_items.size() != g_written_items.size())
    {
        printf("ERROR: number of read items != number of written items.");
        return false;
    }

    if (g_read_items.size() == 0)
    {
        printf("ERROR: number of read items is 0.");
        return false;
    }

    for (std::vector<std::string>::iterator i = g_written_items.begin(); i != g_written_items.end();)
    {
        for (std::vector<std::string>::iterator j = g_read_items.begin(); j != g_read_items.end();)
        {
            if (*i == *j)
            {
                g_written_items.erase(i);
                g_read_items.erase(j);

                i = g_written_items.begin();
                j = g_read_items.begin();
            }
            else
            {
                ++i;
                ++j;
            }
        }
    }

    if (g_read_items.size() != g_written_items.size())
    {
        printf("ERROR: there was a corruption during read/write because not all written items were read.\n");
        
        printf("Leftovers in g_read_items:\n");
        for (size_t i = 0; i < g_read_items.size(); ++i)
            printf("%s ", g_read_items[i].c_str());

        printf("\nLeftovers in g_written_items:\n");
        for (size_t i = 0; i < g_written_items.size(); ++i)
            printf("%s ", g_written_items[i].c_str());
        printf("\n");

        return false;
    }

    if (g_read_items.size() != 0)
    {
        printf("ERROR: read/write problem detected.\n");

        printf("Leftovers in g_read_items:\n");
        for (size_t i = 0; i < g_read_items.size(); ++i)
            printf("%s ", g_read_items[i].c_str());

        printf("\nLeftovers in g_written_items:\n");
        for (size_t i = 0; i < g_written_items.size(); ++i)
            printf("%s ", g_written_items[i].c_str());
        printf("\n");

        return false;
    }

    return true;
}

void run_all_tests()
{
    if (!N_threads_mem_transport_test())
        printf("N_threads_mem_transport_test failed.\n");
        
    printf("tests finished OK.\n");
}

}};

int _tmain(int argc, _TCHAR* argv[])
{
    spipc::testing::run_all_tests();
	return 0;
}
