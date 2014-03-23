#include <stdio.h>
#include <tchar.h>
#include <thread>
#include <atomic>

#include "spipc.h"

namespace spipc { namespace testing {

std::recursive_mutex g_test_mutex;
std::vector<std::string> g_written_items;

void th1_mem_trans_test(Shared_Memory_Transport* trans)
{
    srand(std::this_thread::get_id().hash());

    std::vector<std::string> written_items;
    static std::atomic_int counter{0};
    while (counter++ < 100000)
    {
        int rnd_sz = 1 + rand() % 12;
    
        std::vector<char> to_write;
        for (int i = 0; i < rnd_sz; ++i)
            to_write.push_back(65 + rand() % 20);
        to_write.push_back(0);

        char* write_buf = &(*to_write.begin());
        //printf("Writing: %s (%d bytes)\n", write_buf, rnd_sz + 1);

        trans->write(write_buf, rnd_sz + 1);
        written_items.push_back(write_buf);
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_written_items.insert(g_written_items.end(), written_items.begin(), written_items.end());
}

std::vector<std::string> g_read_items;

void th2_mem_trans_test(Shared_Memory_Transport* trans)
{
    static std::atomic_int counter{0};
    srand(std::this_thread::get_id().hash());
    
    std::vector<std::string> read_items;

    while (counter++ < 100000)
    {
        char read_buf[256];
        memset(read_buf, 0, sizeof(read_buf));
        trans->read(read_buf, sizeof(read_buf), 1000);

        read_items.push_back(read_buf);
        //printf("Reading: %s\n", read_buf);
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_read_items.insert(g_read_items.end(), read_items.begin(), read_items.end());
}

bool N_threads_mem_transport_test()
{
    printf("Running N_threads_mem_transport_test()...\n");
    Shared_Memory_Transport trans;

    std::thread writer(th1_mem_trans_test, &trans);
    std::thread reader(th2_mem_trans_test, &trans);
    std::thread writer2(th1_mem_trans_test, &trans);
    std::thread reader2(th2_mem_trans_test, &trans);
    std::thread writer3(th1_mem_trans_test, &trans);
    std::thread reader3(th2_mem_trans_test, &trans);
    std::thread writer4(th1_mem_trans_test, &trans);
    std::thread writer5(th1_mem_trans_test, &trans);
    std::thread writer6(th1_mem_trans_test, &trans);

    writer.join();
    writer2.join();
    writer3.join();
    writer4.join();
    writer5.join();
    writer6.join();

    //g_stop_reading = true;

    reader.join();
    reader2.join();
    reader3.join();

    if (g_read_items.size() != g_written_items.size())
    {
        printf("ERROR: number of read items (%u) != number of written items (%u).", g_read_items.size(), g_written_items.size());
        return false;
    }

    if (g_read_items.size() == 0)
    {
        printf("ERROR: number of read items is 0.");
        return false;
    }

    for (std::vector<std::string>::iterator i = g_written_items.begin(); i != g_written_items.end();)
    {
        bool increment_i = true;

        for (std::vector<std::string>::iterator j = g_read_items.begin(); j != g_read_items.end();)
        {
            if (*i == *j)
            {
                g_written_items.erase(i);
                g_read_items.erase(j);

                i = g_written_items.begin();
                increment_i = false;

                break;
            }

            ++j;
        }

        if (increment_i)
            ++i;
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
        
    printf("spipc tests finished OK.\n");
}

}};

int _tmain(int argc, _TCHAR* argv[])
{
    spipc::testing::run_all_tests();
	return 0;
}
