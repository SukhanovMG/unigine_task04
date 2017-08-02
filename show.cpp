#include <cstdlib>
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
    unowned_string& operator=(const unowned_string& uo_str)
    {
        m_str = uo_str.c_str();
        m_len = uo_str.size();
        return *this;
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
    mystring(const string& str)
    {
        init_from_buf(str.c_str(), str.size());
    }
    mystring& operator=(const string& str)
    {
        init_from_buf(str.c_str(), str.size());
        return *this;
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
        strncpy(m_str, buf, len);
        m_str[len] = '\0';
        m_len = len;
    }
};

// FILE READING

bool is_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

void to_lower(char& c)
{
    if (c >= 'A' && c <= 'Z')
        c += 32;
}

template <unsigned N, unsigned MAX_WORD_SZ>
class WordReader
{
public:
    WordReader(const char* fname)
        : m_infile(fname, ios_base::binary), m_buffer(nullptr),
          m_begin(nullptr), m_end(nullptr),
          m_cursor(nullptr), m_occupied(0)
    {
        m_buffer = new char[N];
        m_begin = m_buffer;
        m_end = m_buffer + N;
        m_cursor = m_buffer;
        read_next_chunk(m_begin);
    }
    ~WordReader()
    {
        delete[] m_buffer;
    }
    // продолжить чтение файла в буфер, начиная с указателя start
    bool read_next_chunk(char* start)
    {
        m_infile.read(start, m_end - start);
        m_occupied = start - m_begin + m_infile.gcount();
        m_cursor = start;
        return m_occupied > 0;
    }
    // Сдвинуть лежащее на границе слово, начиная со start, в начало буфера
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
        for (char* s = (char*)find_pos; s < m_begin + m_occupied; s++)
        {
            if (!is_char(*s))
                break;
            to_lower(*s);
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
    // Найти очередное слово, в том числе и дочитать файл сколько надо
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
        // Не сработает, если слово не помещается в буфер :(
        if (read_next_chunk(m_cursor))
            find(uo_str);
        return uo_str;
    }

private:
    ifstream m_infile;
    char *m_buffer;
    char *m_begin, *m_end;
    char *m_cursor;
    unsigned m_occupied;
};

// HASH

uint32_t murmur3_32(const char* key, size_t len, uint32_t seed = 0) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h = (h * 5) + 0xe6546b64;
    } while (--i);
    key = (const char*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
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
        : m_max_size(size), m_size(0)
    {
        m_entries = new entry[m_max_size];
    }
    ~hash_table()
    {
        delete[] m_entries;
    }

    unsigned size() const { return m_size; }
    unsigned max_size() const { return m_max_size; }

    unsigned& operator[](const unowned_string& uo_str);
    entry* operator[](unsigned idx);

private:
    entry* m_entries;
    unsigned m_max_size;
    unsigned m_size;
};

// почти как в настоящем контейнере :)
unsigned& hash_table::operator[](const unowned_string& uo_str)
{
    uint32_t hash = murmur3_32(uo_str.c_str(), uo_str.size());
    unsigned index = hash % m_max_size;

    while (m_entries[index].key.c_str() != nullptr)
    {
        if (m_entries[index].key == uo_str)
            return m_entries[index].value;
        index++;
        if (index == m_max_size)
            index = 0;
    }

    m_entries[index].key = uo_str;
    m_entries[index].value = 0;
    m_size++;
    return m_entries[index].value;
}

// очень примерный способ для итерирования
entry* hash_table::operator[](unsigned idx)
{
    if (idx >= m_max_size)
        return nullptr;
    return &m_entries[idx];
}

void sort_and_out(ofstream& ofs, hash_table& h)
{
    entry **entries = new entry*[h.size()];

    for (unsigned i = 0, j = 0; i < h.max_size(); i++)
    {
        if (h[i]->key.c_str() != nullptr)
        {
            entries[j++] = h[i];
        }
    }

    qsort(entries, h.size(), sizeof(entry*), [](const void* l, const void* r)
    {
        const entry *left = *static_cast<const entry* const *>(l);
        const entry *right = *static_cast<const entry* const *>(r);

        if (left->value > right-> value)
            return -1;
        else if (left->value < right->value)
            return 1;
        else
            return strcmp(left->key.c_str(), right->key.c_str());
    });

    for (unsigned i = 0; i < h.size(); i++)
    {
        ofs << entries[i]->value << " " << entries[i]->key.c_str() << endl;
    }

    delete[] entries;
}

int main(int argc, const char *argv[])
{
    try
    {
        if (argc < 3)
        {
            cerr << "Not enough arguments" << endl;
            return 1;
        }
        WordReader<16*1024*1024, 1024> wr(argv[1]);
        ofstream out_file(argv[2]);

        hash_table h(1024*1024);
        unowned_string uo_str;
        while(true)
        {
            uo_str = wr.find_next_word();
            if (uo_str.size() == 0)
                break;
            h[uo_str]++;
        }
        sort_and_out(out_file, h);
    }
    catch (const exception& e)
    {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}