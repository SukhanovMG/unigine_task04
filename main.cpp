#include <cstring>
#include <stdint.h>
#include <iostream>

using namespace std;

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

class unowned_string
{
public:
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
    hash_table h(1024);
    unowned_string str1("ololo", strlen("ololo"));
    unowned_string str2("eisenhower", strlen("eisenhower"));
    unowned_string str3("petfood", strlen("petfood"));

    h[str1] = 1;
    h[str2] = 2;
    h[str3] = 3;

    cout << str1.c_str() << " >> " << h[str1] << endl;
    cout << str2.c_str() << " >> " << h[str2] << endl;
    cout << str3.c_str() << " >> " << h[str3] << endl;
    return 0;
}