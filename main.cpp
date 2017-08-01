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

template <unsigned N, unsigned MAX_WORD_SZ>
class WordReader
{
public:
    WordReader(const char* fname)
        : m_infile(fname, ios_base::binary), m_buffer{0},
          m_begin(m_buffer), m_end(m_buffer + N),
          m_cursor(m_buffer), m_occupied(0)
    {
        read_next_chunk(m_begin);
    }
    // продолжить чтение файла в буфер, начиная с указателя start
    bool read_next_chunk(char* start)
    {
        m_infile.read(start, m_end - start);
        m_occupied = start - m_begin + m_infile.gcount();
        m_cursor = start;
        return m_occupied > 0;
    }
    void move_to_begin(const char* start)
    {
        unsigned pos = start - m_begin;
        for (unsigned i = 0, j = pos; j < m_occupied; i++, j++)
        {
            m_buffer[i] = m_buffer[j];
        }
        if (m_occupied >= pos)
        {
            m_occupied-=pos;
            m_cursor = m_begin + m_occupied;
        }
    }
    // Найти в заполненном буфере слово или продолжить поиск уже найденного слова
    // Возвращает были ли найден хоть один символ для слова
    bool find(unowned_string& hint)
    {
        // Определение начальных данных для поиска.
        // Может быть первый поиск слова, тогда ищем слово, начиная с hint.c_str(), а hint.size() == 0
        // Может быть поиск продолжения слова (для слова найденного на границе буфера),
        // тогда ищем с hint.c_str() + hint.size()
        const char* find_pos = hint.c_str() + hint.size();
        unsigned len = hint.size();
        unsigned len_before = len;
        bool continued_find = len > 0;
        // Если не продолжение поиска, то нужно сдвинуться до первого подходящего симвовла
        if (!continued_find)
        {
            while(!is_char(*find_pos) && find_pos != m_begin + m_occupied)
                find_pos++;
        }
        // Перемещение курсора для последующего поиска
        m_cursor = (char*)find_pos;
        // Здесь наращиваем длину слова до первой "не буквы" или до конца буфера
        for (const char* s = find_pos; s < m_begin + m_occupied; s++)
        {
            if (!is_char(*s))
                break;
            // Ограничение на максимальную длину слова
            if (len < MAX_WORD_SZ)
                len++;
            // Курсор сдвивагается всегда, чтобы обрезать длинное слово
            m_cursor++;
        }
        if (continued_find)
            hint = unowned_string(hint.c_str(), len);
        else
            hint = unowned_string(find_pos, len);
        return len - len_before != 0;
    }
    unowned_string find_next_word()
    {
        unowned_string uo_str(m_cursor, 0);
        // Фаза 1: Найти начало очередного слова
        while(!find(uo_str))
        {
            if (!read_next_chunk(m_begin))
                return uo_str;
            uo_str = unowned_string(m_cursor, 0);
        }
        // Фаза 2: Если слово не на границе, то поиск завершён
        if (m_cursor != m_begin + m_occupied)
            return uo_str;
        // Фаза 3: Перемещение слова из конца буфера в начало
        move_to_begin(uo_str.c_str());
        uo_str = unowned_string(m_begin, uo_str.size());
        // Фаза 4: Дочитывание из файла и поиск продолжения слова
        while (true)
        {
            if (!read_next_chunk(m_cursor))
                break;
            if (!find(uo_str))
                break;
        }
        // Пока не хватает ограничения на максимальную длину слова
        return uo_str;
    }

//private:
    ifstream m_infile;
    char m_buffer[N];
    char *m_begin, *m_end;
    char *m_cursor;
    unsigned m_occupied;
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
    entry* operator[](unsigned idx);

private:
    entry* m_entries;
    unsigned m_size;
};

unsigned& hash_table::operator[](const unowned_string& uo_str)
{
    //cout << uo_str.c_str() << endl;
    uint32_t crc = crc32c(uo_str.c_str(), uo_str.size());
    //cout << "crc: " << hex << crc << dec << endl;
    unsigned index = crc % m_size;
    //cout << "index: " << index << endl;

    while (m_entries[index].key.c_str() != nullptr)
    {
        //cout << "slot[" << index << "]: " << (m_entries[index].key.c_str() == nullptr ? "nullptr" : m_entries[index].key.c_str()) << endl;
        if (m_entries[index].key == uo_str)
            return m_entries[index].value;
        index++;
    }

    //cout << "inserting in slot " << index << endl;
    m_entries[index].key = uo_str;
    m_entries[index].value = 0;
    return m_entries[index].value;
}

entry* hash_table::operator[](unsigned idx)
{
    if (idx >= m_size)
        return nullptr;
    return &m_entries[idx];
}

int main()
{
    hash_table h(1024);

    //WordReader<16, 5> wr("build/in.txt");

    // while(true)
    // {
    //     unowned_string uo_str = wr.find_next_word();
    //     if (uo_str.size() == 0)
    //         break;
    //     cout.write(uo_str.c_str(), uo_str.size());
    //     cout << endl;
    // }

    unowned_string str1("ololo", strlen("ololo"));
    unowned_string str2("eisenhower", strlen("eisenhower"));
    unowned_string str3("petfood", strlen("petfood"));

    h[str1] = 1;
    h[str2] = 2;
    h[str3] = 3;

    cout << str1.c_str() << " >> " << h[str1] << endl;
    cout << str2.c_str() << " >> " << h[str2] << endl;
    cout << str3.c_str() << " >> " << h[str3] << endl;

    for (unsigned i = 0; i < h.size(); i++)
    {
        if (h[i]->key.c_str() != nullptr)
            cout << h[i]->key.c_str() << ": " << h[i]->value << endl;
    }
    return 0;
}