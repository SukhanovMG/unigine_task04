#include <cstring>
#include <stdint.h>
#include <iostream>
#include <fstream>

using namespace std;

// STRINGS

class unowned_string
{
public:
    unowned_string()
        : m_str(nullptr), m_len(0)
    {
    }
    unowned_string(const char *str, unsigned len)
        : m_str(str), m_len(len)
    {
    }
    ~unowned_string() {}
    const char *c_str() const { return m_str; }
    unsigned size() const { return m_len; }
private:
    const char *m_str;
    unsigned m_len;
};

class mystring
{
public:
    mystring()
        : m_str(nullptr), m_len(0)
    {
    }
    mystring(const char* str, unsigned len)
    {
        init_from_buf(str, len);
    }
    mystring(const unowned_string& uo_str)
    {
        init_from_buf(uo_str.c_str(), uo_str.size());
    }
    mystring& operator=(const unowned_string& uo_str)
    {
        init_from_buf(uo_str.c_str(), uo_str.size());
        return *this;
    }
    bool operator==(const unowned_string& rhs)
    {
        return strncmp(m_str, rhs.c_str(), rhs.size()) == 0;
    }
    bool operator!=(const unowned_string& rhs)
    {
        return !(*this == rhs);
    }
    ~mystring()
    {
        delete[] m_str;
    }

    const char *c_str() const { return m_str; }
    unsigned size() const { return m_len; }
private:
    char* m_str;
    unsigned m_len;

    void init_from_buf(const char* buf, unsigned len)
    {
        m_str = new char[len+1];
        strncpy(m_str, buf, len+1);
        m_len = len;
    }
};

// FILE READING

bool is_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

template <unsigned N>
class WordReader
{
public:
    WordReader(const char* fname)
        : infile(fname, ios_base::binary), buffer{0}, occupied(0), find_pos(0)
    {
    }
    // продолжить чтение файла в буфер, начиная с позиции pos
    bool read_next_chunk(unsigned pos = 0)
    {
        infile.read(buffer + pos, N - pos);
        occupied = pos + infile.gcount();
        find_pos = 0;
        return occupied > pos;
    }
    void move_to_begin(unsigned pos)
    {
        for (unsigned i = 0, j = pos; j < occupied; i++, j++)
        {
            buffer[i] = buffer[j];
        }
        if (occupied >= pos)
            occupied-=pos;
    }
    unowned_string find_next_word()
    {
        const char *str = nullptr;
        unsigned len = 0;
        for(unsigned i = find_pos; i < occupied; i++)
        {
            find_pos++;
            if (is_char(buffer[i]))
            {
                if (str == nullptr)
                    str = &buffer[i];
                len++;
            }
            else
            {
                if (str != nullptr)
                    break;
            }
        }
        return unowned_string(str, len);
    }
//private:
    ifstream infile;
    char buffer[N];
    unsigned occupied;
    unsigned find_pos;
};

// HASH

#define POLY 0xedb88320

uint32_t crc32c(const char *buf, size_t len, uint32_t crc = 0)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

struct entry
{
    entry()
        : key(), value(0)
    {
    }
    entry(const unowned_string& uo_str)
        : key(uo_str), value(0)
    {
    }
    mystring key;
    unsigned value;
};

class hash_table
{
public:
    hash_table(unsigned size)
        : m_size(size)
    {
        m_entries = new entry[size];
    }
    ~hash_table()
    {
        delete[] m_entries;
    }

    unsigned size() const { return m_size; }

    unsigned& operator[](const unowned_string& uo_str);

private:
    entry* m_entries;
    unsigned m_size;
};

unsigned& hash_table::operator[](const unowned_string& uo_str)
{
    cout << uo_str.c_str() << endl;
    uint32_t crc = crc32c(uo_str.c_str(), uo_str.size());
    cout << "crc: " << hex << crc << dec << endl;
    unsigned index = crc % m_size;
    cout << "index: " << index << endl;

    while (m_entries[index].key.c_str() != nullptr)
    {
        cout << "slot[" << index << "]: " << (m_entries[index].key.c_str() == nullptr ? "nullptr" : m_entries[index].key.c_str()) << endl;
        if (m_entries[index].key == uo_str)
            return m_entries[index].value;
        index++;
    }

    cout << "inserting in slot " << index << endl;
    m_entries[index].key = uo_str;
    m_entries[index].value = 0;
    return m_entries[index].value;
}

int main()
{
    //hash_table h(1024);

    WordReader<16> wr("build/in.txt");

    wr.read_next_chunk(1);
    for (unsigned i = 0; i < wr.occupied; i++)
    {
        cout << hex << (unsigned)wr.buffer[i] << dec << endl;
    }
    cout << "====" << endl;
    wr.move_to_begin(14);
    wr.read_next_chunk(16-14);
    for (unsigned i = 0; i < wr.occupied; i++)
    {
        cout << hex << (unsigned)wr.buffer[i] << dec << endl;
    }

    // while (wr.read_next_chunk())
    // {
    //     unowned_string uo_str;
    //     do
    //     {
    //         uo_str = wr.find_next_word();
    //         if (uo_str.c_str() == nullptr)
    //             break;
    //         cout.write(uo_str.c_str(), uo_str.size());
    //         cout << endl;
    //     } while (true);
    //     cout << "=======" << endl;
    // }

    // unowned_string str1("ololo", strlen("ololo"));
    // unowned_string str2("eisenhower", strlen("eisenhower"));
    // unowned_string str3("petfood", strlen("petfood"));

    // h[str1] = 1;
    // h[str2] = 2;
    // h[str3] = 3;

    // cout << str1.c_str() << " >> " << h[str1] << endl;
    // cout << str2.c_str() << " >> " << h[str2] << endl;
    // cout << str3.c_str() << " >> " << h[str3] << endl;
    return 0;
}