#include <stdio.h>
#include <tchar.h>
#include <thread>
#include <atomic>

#include "spipc.h"
#include "shared_memory_transport.h"

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
    
    std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
    long long begin = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();

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

    long long end = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();

    printf("Shared mem transport delivers %g messages per second.\n", g_read_items.size() * 1.0 / (end * 1.0 - begin * 1.0) * 1000);

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

std::map<spipc::UID, std::vector<std::string>> g_ipc_written;
std::map<spipc::UID, std::vector<std::string>> g_ipc_read;

void writer_ipc_thread(const spipc::UID uid)
{
    srand(std::this_thread::get_id().hash());
    std::vector<std::string> ipc_written;

    spipc::IPC ipc;

    ipc.connect(uid);

    char fwriter_name[255];
    sprintf_s(fwriter_name, "writer_%lld_%lld.txt", uid.high, uid.low);
    FILE* fwriter = 0;
    fopen_s(&fwriter, fwriter_name, "w");

    int prefix = 0;

    if (uid.high == 18743) prefix = 0;
    if (uid.high == 18745) prefix = 3;
    if (uid.high == 18747) prefix = 6;
    
    char number[100];

    for (int i = 0; i < 15000; ++i)
    {
        sprintf_s(number, "%d", i);
        int rnd_sz = 1 + rand() % 1295;
    
        std::vector<char> to_write;
        for (int j = 0; j < rnd_sz; ++j)
            to_write.push_back(65 + rand() % 3 + prefix);
        to_write.push_back('_');
        int num_len = strlen(number);
        for (int k = 0; k < num_len; k++)
            to_write.push_back(number[k]);
        to_write.push_back('_');
        to_write.push_back(0);

        char* write_buf = &(*to_write.begin());
        //printf("Writing: %s (%d bytes)\n", write_buf, rnd_sz + 1);
    retry_write:

        try
        {
            ipc.write(write_buf, to_write.size(), 3000);
        }
        catch(fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION in reader_ipc_thread: %s\n", e.what().c_str());
            goto retry_write;
        }
        catch(sprot::exceptions::Exception& e)
        {
            printf("EXCEPTION in writer_ipc_thread: %s\n", e.what().c_str());
            goto retry_write;
        }
            
        ipc_written.push_back(write_buf);

        char newline[2] = { 0x0d, 0x0a };
        fwrite(write_buf, to_write.size() - 1, 1, fwriter);
        fwrite(newline, 2, 1, fwriter);
        fflush(fwriter);
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_ipc_written[uid] = ipc_written;
    
    fclose(fwriter);
    printf("thread with UID [%lld/%lld] written %d values.\n", uid.high, uid.low, ipc_written.size());
}

void reader_ipc_thread(const spipc::UID uid)
{
    srand(std::this_thread::get_id().hash());
    std::vector<std::string> read_items;

    spipc::IPC ipc;
    ipc.connect(uid);
    
    char freader_name[255];
    sprintf_s(freader_name, "reader_%lld_%lld.txt", uid.high, uid.low);
    FILE* freader = 0;
    fopen_s(&freader, freader_name, "w");

    for (int i = 0; i < 15000; ++i)
    {
        char read_buf[3000];
        memset(read_buf, 0, sizeof(read_buf));
    
    retry_read:

        try
        {
            ipc.read(read_buf, sizeof(read_buf), 3000);
        }
        catch(fplog::exceptions::Generic_Exception& e)
        {
            printf("EXCEPTION in reader_ipc_thread: %s\n", e.what().c_str());
            goto retry_read;
        }
        catch(sprot::exceptions::Exception& e)
        {
            printf("EXCEPTION in reader_ipc_thread: %s\n", e.what().c_str());
            goto retry_read;
        }

        read_items.push_back(read_buf);

        char newline[2] = { 0x0d, 0x0a };
        fwrite(read_buf, strlen(read_buf), 1, freader);
        fwrite(newline, 2, 1, freader);
        fflush(freader);

        //printf("Reading: %s\n", read_buf);
    }

    std::lock_guard<std::recursive_mutex> lock(g_test_mutex);
    g_ipc_read[uid] = read_items;

    fclose(freader);
    printf("thread with UID [%lld/%lld] read %d values.\n", uid.high, uid.low, read_items.size());
}

bool N_threads_IPC_test()
{
    spipc::UID uid;
    std::chrono::time_point<std::chrono::system_clock> beginning_of_time(std::chrono::system_clock::from_time_t(0));
    long long begin = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();

    uid.high = 18743;
    uid.low = 18744;
    std::thread writer(writer_ipc_thread, uid);


    uid.high = 18745;
    uid.low = 18746;
    std::thread writer2(writer_ipc_thread, uid);

    uid.high = 18747;
    uid.low = 18748;
    std::thread writer3(writer_ipc_thread, uid);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    uid.high = 18743;
    uid.low = 18744;
    std::thread reader(reader_ipc_thread, uid);

    uid.high = 18745;
    uid.low = 18746;
    std::thread reader2(reader_ipc_thread, uid);

    uid.high = 18747;
    uid.low = 18748;
    std::thread reader3(reader_ipc_thread, uid);

    writer.join();
    reader.join();

    writer2.join();
    reader2.join();

    writer3.join();
    reader3.join();

    long long end = (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::duration(std::chrono::system_clock::now() - beginning_of_time))).count();
    size_t read_messages = 0;

    std::map<spipc::UID, std::vector<std::string>>::iterator it(g_ipc_written.begin());
    for (; it != g_ipc_written.end(); ++it)
    {
        std::vector<std::string> v1(g_ipc_read[it->first]);
        read_messages += v1.size();

        if (v1 != it->second)
        {
            printf("ERROR: IPC read/write problem detected - written items did not match read items.\n");
            return false;
        }
    }

    printf("IPC delivers %g messages per second.\n", read_messages * 1.0 / (end * 1.0 - begin * 1.0) * 1000);
    return true;
}

void buffer_overflow_test_worker()
{
    char data_5mb[5 * 1024 * 1024] = {0};
    for (int i = 0; i < sizeof(data_5mb); ++i)
        data_5mb[i] = 65 + i % 23;

    spipc::IPC buffer_overflow_test;
    spipc::UID uid;
    uid.high = 18743;
    uid.low = 18744;
    buffer_overflow_test.connect(uid);
    buffer_overflow_test.write(data_5mb, sizeof(data_5mb));
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}

bool Buffer_Overflow_Test()
{
    char data_5mb[5 * 1024 * 1024] = {0};
    for (int i = 0; i < sizeof(data_5mb); ++i)
        data_5mb[i] = 65 + i % 23;

    size_t read_buf_sz = 256 * 1024;
    char* read_buf = new char[read_buf_sz];

    spipc::IPC buffer_overflow_test;
    spipc::UID uid;
    uid.high = 18743;
    uid.low = 18744;
    buffer_overflow_test.connect(uid);

    std::thread worker(&buffer_overflow_test_worker);

    int retries = 0;
retry:

    if (retries > 1)
    {
        printf("buffer_overflow_test.read() failed.\n");
        return false;
    }

    try
    {
        buffer_overflow_test.read(read_buf, read_buf_sz);
        if ((sizeof(data_5mb) != read_buf_sz) || memcmp(read_buf, data_5mb, sizeof(data_5mb)) != 0)
        {
            printf("buffer_overflow_test.read() got corrupted data.\n");
            return false;
        }
    }
    catch(fplog::exceptions::Read_Failed)
    {
        printf("buffer_overflow_test.read() failed.\n");
        return false;
    }
    catch(fplog::exceptions::Buffer_Overflow& e)
    {
        retries++;
        delete read_buf;
        read_buf_sz = sizeof(data_5mb);
        read_buf = new char[read_buf_sz];
        goto retry;
    }

    worker.join();

    return true;
}

void run_all_tests()
{
    spipc::Shared_Memory_Transport::global_init();

    try
    {
        if (!N_threads_mem_transport_test())
            printf("N_threads_mem_transport_test failed.\n");
    }
    catch (sprot::exceptions::Exception& e)
    {
        printf("ERROR: N_threads_mem_transport_test failed with exception.\n");
        printf("%s\n", e.what().c_str());
    }

    try
    {
        if (!N_threads_IPC_test())
            printf("N_threads_IPC_test failed.\n");
    }
    catch (sprot::exceptions::Exception& e)
    {
        printf("ERROR: N_threads_IPC_test failed with exception.\n");
        printf("%s\n", e.what().c_str());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    try
    {
        if (!Buffer_Overflow_Test())
            printf("Buffer_Overflow_Test failed.\n");
    }
    catch (sprot::exceptions::Exception& e)
    {
        printf("ERROR: Buffer_Overflow_Test failed with exception.\n");
        printf("%s\n", e.what().c_str());
    }

    printf("spipc tests finished OK.\n");
}

}};

int _tmain(int argc, _TCHAR* argv[])
{
    spipc::testing::run_all_tests();
	return 0;
}
