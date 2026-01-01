#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <string>

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <cerrno>
#include <iostream>
using namespace std;

struct BPlusKey
{
    char data[128];

    BPlusKey()
    {
        memset(data, 0, sizeof(data));
    }

    BPlusKey(const string &str)
    {
        memset(data, 0, sizeof(data));
        strncpy(data, str.c_str(), sizeof(data) - 1);
    }

    string to_string() const
    {
        return string(data);
    }

    bool operator<(const BPlusKey &other) const
    {
        return strcmp(data, other.data) < 0;
    }

    bool operator==(const BPlusKey &other) const
    {
        return strcmp(data, other.data) == 0;
    }

    bool operator<=(const BPlusKey &other) const
    {
        return strcmp(data, other.data) <= 0;
    }

    bool operator>(const BPlusKey &other) const
    {
        return strcmp(data, other.data) > 0;
    }

    bool operator>=(const BPlusKey &other) const
    {
        return strcmp(data, other.data) >= 0;
    }
};

struct BPlusValue
{
    uint64_t primary_id;
    uint8_t entity_type;
    uint8_t reserved[7];

    BPlusValue() : primary_id(0), entity_type(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }

    BPlusValue(uint64_t id, uint8_t type) : primary_id(id), entity_type(type)
    {
        memset(reserved, 0, sizeof(reserved));
    }
};

struct BPlusNode
{
    static constexpr int ORDER = 10;
    static constexpr int MAX_KEYS = 19;
    static constexpr int MIN_KEYS = 9;
    static constexpr int MAX_CHILDREN = 20;

    uint8_t node_type;
    uint16_t key_count;
    uint64_t parent_offset;
    uint16_t level;
    uint8_t padding[45];

    BPlusKey keys[MAX_KEYS];

    union
    {
        uint64_t child_offsets[MAX_CHILDREN];
        BPlusValue values[MAX_KEYS];
    };

    uint64_t next_leaf;
    uint64_t prev_leaf;

    uint8_t node_padding[1280];

    BPlusNode() : node_type(1), key_count(0), parent_offset(0), level(0),
                  next_leaf(0), prev_leaf(0)
    {
        memset(padding, 0, sizeof(padding));
        memset(node_padding, 0, sizeof(node_padding));
        for (int i = 0; i < MAX_CHILDREN; i++)
        {
            child_offsets[i] = 0;
        }
    }

    bool is_leaf() const { return node_type == 1; }
    bool is_full() const { return key_count >= MAX_KEYS; }
};

static_assert(sizeof(BPlusNode) == 4096, "BPlusNode must be 4096 bytes");

struct BPlusMetadata
{
    char magic[8];
    char index_name[64];
    uint64_t root_offset;
    uint64_t leftmost_leaf;
    uint64_t total_entries;
    uint32_t tree_height;
    uint8_t reserved[3996];

    BPlusMetadata() : root_offset(0), leftmost_leaf(0), total_entries(0), tree_height(0)
    {
        strncpy(magic, "BPLUS001", 8);
        memset(index_name, 0, sizeof(index_name));
        memset(reserved, 0, sizeof(reserved));
    }
};

static_assert(sizeof(BPlusMetadata) == 4096, "BPlusMetadata must be 4096 bytes");

class BPlusTree
{
private:
    fstream file_;
    string filename_;
    BPlusMetadata metadata_;

    bool read_node(uint64_t offset, BPlusNode &node)
    {
        if (offset == 0)
            return false;
        file_.seekg(offset, ios::beg);
        file_.read(reinterpret_cast<char *>(&node), sizeof(BPlusNode));
        return file_.good();
    }

    bool write_node(uint64_t offset, const BPlusNode &node)
    {
        file_.seekp(offset, ios::beg);
        file_.write(reinterpret_cast<const char *>(&node), sizeof(BPlusNode));
        file_.flush();
        return file_.good();
    }

    uint64_t allocate_node()
    {
        file_.seekp(0, ios::end);
        uint64_t offset = file_.tellp();
        BPlusNode empty;
        file_.write(reinterpret_cast<char *>(&empty), sizeof(BPlusNode));
        return offset;
    }

    int find_key_position(const BPlusNode &node, const BPlusKey &key)
    {
        int pos = 0;
        while (pos < node.key_count && node.keys[pos] < key)
        {
            pos++;
        }
        return pos;
    }

    void split_child(uint64_t parent_offset, BPlusNode &parent, int index)
    {
        BPlusNode child;
        read_node(parent.child_offsets[index], child);

        uint64_t new_offset = allocate_node();
        BPlusNode new_node;
        new_node.node_type = child.node_type;
        new_node.level = child.level;

        int mid = BPlusNode::MIN_KEYS;
        new_node.key_count = BPlusNode::MIN_KEYS;

        for (int i = 0; i < BPlusNode::MIN_KEYS; i++)
        {
            new_node.keys[i] = child.keys[mid + 1 + i];
        }

        if (child.is_leaf())
        {
            for (int i = 0; i < BPlusNode::MIN_KEYS; i++)
            {
                new_node.values[i] = child.values[mid + 1 + i];
            }
            new_node.next_leaf = child.next_leaf;
            child.next_leaf = new_offset;
            new_node.prev_leaf = parent.child_offsets[index];
        }
        else
        {
            for (int i = 0; i <= BPlusNode::MIN_KEYS; i++)
            {
                new_node.child_offsets[i] = child.child_offsets[mid + 1 + i];
            }
        }

        child.key_count = BPlusNode::MIN_KEYS;

        for (int i = parent.key_count; i > index; i--)
        {
            parent.keys[i] = parent.keys[i - 1];
            parent.child_offsets[i + 1] = parent.child_offsets[i];
        }

        parent.keys[index] = child.keys[mid];
        parent.child_offsets[index + 1] = new_offset;
        parent.key_count++;

        write_node(parent.child_offsets[index], child);
        write_node(new_offset, new_node);
        write_node(parent_offset, parent);
    }

    void insert_non_full(uint64_t node_offset, BPlusNode &node,
                         const BPlusKey &key, const BPlusValue &value)
    {
        int pos = node.key_count - 1;

        if (node.is_leaf())
        {
            while (pos >= 0 && node.keys[pos] > key)
            {
                node.keys[pos + 1] = node.keys[pos];
                node.values[pos + 1] = node.values[pos];
                pos--;
            }
            node.keys[pos + 1] = key;
            node.values[pos + 1] = value;
            node.key_count++;
            write_node(node_offset, node);
        }
        else
        {
            while (pos >= 0 && node.keys[pos] > key)
                pos--;
            pos++;

            BPlusNode child;
            read_node(node.child_offsets[pos], child);

            if (child.is_full())
            {
                split_child(node_offset, node, pos);
                if (node.keys[pos] < key)
                    pos++;
                read_node(node.child_offsets[pos], child);
            }

            insert_non_full(node.child_offsets[pos], child, key, value);
        }
    }

    bool search_recursive(uint64_t node_offset, const BPlusKey &key, BPlusValue &result)
    {
        if (node_offset == 0)
            return false;

        BPlusNode node;
        read_node(node_offset, node);

        int pos = find_key_position(node, key);

        if (node.is_leaf())
        {
            if (pos < node.key_count && node.keys[pos] == key)
            {
                result = node.values[pos];
                return true;
            }
            return false;
        }

        return search_recursive(node.child_offsets[pos], key, result);
    }

public:
    BPlusTree(const string &filename, const string &index_name)
        : filename_(filename)
    {
        strncpy(metadata_.index_name, index_name.c_str(), sizeof(metadata_.index_name) - 1);
    }
    ~BPlusTree()
    {
        close();
    }

    bool create()
    {

        if (file_.is_open())
        {
            file_.close();
        }

        cout << "          Creating: " << filename_ << endl;

        file_.open(filename_, ios::out | ios::binary | ios::trunc);
        if (!file_.is_open())
        {
            cerr << "          ERROR: Cannot create file: " << filename_ << endl;
            cerr << "          Error: " << strerror(errno) << endl;
            return false;
        }

        file_.write(reinterpret_cast<char *>(&metadata_), sizeof(BPlusMetadata));

        if (!file_.good())
        {
            cerr << "          ERROR: Failed to write metadata!" << endl;
            file_.close();
            return false;
        }

        BPlusNode root;
        metadata_.root_offset = sizeof(BPlusMetadata);
        metadata_.leftmost_leaf = metadata_.root_offset;

        file_.write(reinterpret_cast<char *>(&root), sizeof(BPlusNode));

        if (!file_.good())
        {
            cerr << "          ERROR: Failed to write root node!" << endl;
            file_.close();
            return false;
        }

        file_.flush();

        file_.seekp(0);
        file_.write(reinterpret_cast<char *>(&metadata_), sizeof(BPlusMetadata));
        file_.flush();

        cout << "          Created successfully" << endl;
        return true;
    }

    bool open()
    {

        if (file_.is_open())
        {
            cout << "          File already open, verifying..." << endl;
            file_.seekg(0, ios::beg);
            file_.read(reinterpret_cast<char *>(&metadata_), sizeof(BPlusMetadata));

            if (string(metadata_.magic, 8) != "BPLUS001")
            {
                cerr << "          ERROR: Invalid magic number!" << endl;
                file_.close();
                return false;
            }

            return true;
        }

        cout << "          Opening: " << filename_ << endl;

        file_.open(filename_, ios::in | ios::out | ios::binary);
        if (!file_.is_open())
        {
            cerr << "          ERROR: Cannot open file: " << filename_ << endl;
            return false;
        }

        file_.read(reinterpret_cast<char *>(&metadata_), sizeof(BPlusMetadata));

        bool valid = (string(metadata_.magic, 8) == "BPLUS001");
        if (!valid)
        {
            cerr << "          ERROR: Invalid BPlusTree file" << endl;
            file_.close();
        }

        return valid;
    }
    void close()
    {
        if (file_.is_open())
        {
            file_.seekp(0);
            file_.write(reinterpret_cast<char *>(&metadata_), sizeof(BPlusMetadata));
            file_.close();
        }
    }

    bool insert(const BPlusKey &key, const BPlusValue &value)
    {
        BPlusNode root;
        read_node(metadata_.root_offset, root);

        if (root.is_full())
        {
            uint64_t new_root_offset = allocate_node();
            BPlusNode new_root;
            new_root.node_type = 0;
            new_root.level = root.level + 1;
            new_root.child_offsets[0] = metadata_.root_offset;

            split_child(new_root_offset, new_root, 0);
            metadata_.root_offset = new_root_offset;
            metadata_.tree_height++;

            read_node(new_root_offset, new_root);
            insert_non_full(new_root_offset, new_root, key, value);
        }
        else
        {
            insert_non_full(metadata_.root_offset, root, key, value);
        }

        metadata_.total_entries++;
        return true;
    }

    bool search(const BPlusKey &key, BPlusValue &result)
    {
        return search_recursive(metadata_.root_offset, key, result);
    }

    vector<pair<BPlusKey, BPlusValue>> scan_all()
    {
        vector<pair<BPlusKey, BPlusValue>> results;

        uint64_t current = metadata_.leftmost_leaf;
        while (current != 0)
        {
            BPlusNode leaf;
            read_node(current, leaf);

            for (int i = 0; i < leaf.key_count; i++)
            {
                results.push_back({leaf.keys[i], leaf.values[i]});
            }

            current = leaf.next_leaf;
        }

        return results;
    }

    uint64_t get_total_entries() const { return metadata_.total_entries; }
};
#endif