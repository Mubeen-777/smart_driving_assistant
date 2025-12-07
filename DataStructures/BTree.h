#ifndef BTREE_H
#define BTREE_H


#include <cstdint>
#include <cstring>
#include <vector>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <cerrno>
#include <iostream>

struct CompositeKey
{
    uint8_t entity_type; 
    uint64_t primary_id; 
    uint64_t timestamp;  
    uint32_t sequence;  

    CompositeKey() : entity_type(0), primary_id(0), timestamp(0), sequence(0) {}

    CompositeKey(uint8_t type, uint64_t id, uint64_t ts, uint32_t seq = 0)
        : entity_type(type), primary_id(id), timestamp(ts), sequence(seq) {}

    bool operator<(const CompositeKey &other) const
    {
        if (entity_type != other.entity_type)
            return entity_type < other.entity_type;
        if (primary_id != other.primary_id)
            return primary_id < other.primary_id;
        if (timestamp != other.timestamp)
            return timestamp < other.timestamp;
        return sequence < other.sequence;
    }

    bool operator==(const CompositeKey &other) const
    {
        return entity_type == other.entity_type &&
               primary_id == other.primary_id &&
               timestamp == other.timestamp &&
               sequence == other.sequence;
    }

    bool operator<=(const CompositeKey &other) const
    {
        return *this < other || *this == other;
    }

    bool operator>(const CompositeKey &other) const
    {
        return !(*this <= other);
    }

    bool operator>=(const CompositeKey &other) const
    {
        return !(*this < other);
    }
};

struct BTreeValue
{
    uint64_t record_offset; 
    uint8_t file_id;        
    uint16_t record_size;   
    uint8_t reserved[5];    

    BTreeValue() : record_offset(0), file_id(0), record_size(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }

    BTreeValue(uint64_t offset, uint8_t fid, uint16_t size)
        : record_offset(offset), file_id(fid), record_size(size)
    {
        memset(reserved, 0, sizeof(reserved));
    }
};
struct BTreeNode
{
    static constexpr int ORDER = 5;         
    static constexpr int MAX_KEYS = 9;      
    static constexpr int MIN_KEYS = 4;      
    static constexpr int MAX_CHILDREN = 10; 

    
    uint8_t node_type;          
    uint16_t key_count;         
    uint64_t parent_offset;     
    uint16_t level;             
    uint8_t dirty_flag;         
    uint16_t crc16;             
    uint8_t header_padding[48]; 

    
    CompositeKey keys[MAX_KEYS];

    
    union
    {
        BTreeValue values[MAX_CHILDREN];      
        uint64_t child_offsets[MAX_CHILDREN]; 
    };

    
    uint64_t next_leaf; 
    uint64_t prev_leaf; 

    
    
    
    uint8_t padding[3480];

    BTreeNode() : node_type(1), key_count(0), parent_offset(0), level(0),
                  dirty_flag(0), crc16(0), next_leaf(0), prev_leaf(0)
    {
        memset(header_padding, 0, sizeof(header_padding));
        memset(padding, 0, sizeof(padding));
        for (int i = 0; i < MAX_CHILDREN; i++)
        {
            child_offsets[i] = 0;
        }
    }

    bool is_leaf() const { return node_type == 1; }
    bool is_full() const { return key_count >= MAX_KEYS; }
    bool is_underflow() const { return key_count < MIN_KEYS; }
};
static_assert(sizeof(BTreeNode) == 4096, "BTreeNode must be exactly 4096 bytes");

struct BTreeMetadata
{
    char magic[8];            
    uint32_t version;         
    uint64_t root_offset;     
    uint64_t total_records;   
    uint32_t tree_height;     
    uint64_t free_list_head;  
    uint64_t last_compaction; 
    uint8_t reserved[4040];   


    BTreeMetadata() : version(1), root_offset(0), total_records(0),
                      tree_height(0), free_list_head(0), last_compaction(0)
    {
        strncpy(magic, "BTREE001", 8);
        memset(reserved, 0, sizeof(reserved));
    }
};
static_assert(sizeof(BTreeMetadata) == 4096, "BTreeMetadata must be 4096 bytes");

class BTree
{
private:
    fstream file_;
    string filename_;
    BTreeMetadata metadata_;

    struct CacheEntry
    {
        uint64_t offset;
        BTreeNode node;
        bool dirty;
    };
    vector<CacheEntry> cache_;
    static constexpr size_t CACHE_SIZE = 256;

    bool read_node(uint64_t offset, BTreeNode &node)
    {
        if (offset == 0)
            return false;

        for (auto &entry : cache_)
        {
            if (entry.offset == offset)
            {
                node = entry.node;
                return true;
            }
        }

        file_.seekg(offset, ios::beg);
        file_.read(reinterpret_cast<char *>(&node), sizeof(BTreeNode));

        if (!file_.good())
            return false;

        add_to_cache(offset, node, false);
        return true;
    }

    bool write_node(uint64_t offset, const BTreeNode &node)
    {
        if (offset == 0)
            return false;

        file_.seekp(offset, ios::beg);
        file_.write(reinterpret_cast<const char *>(&node), sizeof(BTreeNode));
        file_.flush();

        update_cache(offset, node, false);
        return file_.good();
    }

    uint64_t allocate_node()
    {
        
        file_.seekp(0, ios::end);
        uint64_t offset = file_.tellp();

        BTreeNode empty_node;
        file_.write(reinterpret_cast<char *>(&empty_node), sizeof(BTreeNode));
        file_.flush();

        return offset;
    }

    
    void add_to_cache(uint64_t offset, const BTreeNode &node, bool dirty)
    {
        if (cache_.size() >= CACHE_SIZE)
        {
            
            if (cache_[0].dirty)
            {
                write_node(cache_[0].offset, cache_[0].node);
            }
            cache_.erase(cache_.begin());
        }
        cache_.push_back({offset, node, dirty});
    }

    
    void update_cache(uint64_t offset, const BTreeNode &node, bool dirty)
    {
        for (auto &entry : cache_)
        {
            if (entry.offset == offset)
            {
                entry.node = node;
                entry.dirty = dirty;
                return;
            }
        }
        add_to_cache(offset, node, dirty);
    }

    
    int find_key_position(const BTreeNode &node, const CompositeKey &key)
    {
        int pos = 0;
        while (pos < node.key_count && node.keys[pos] < key)
        {
            pos++;
        }
        return pos;
    }

    
    void split_child(uint64_t parent_offset, BTreeNode &parent, int child_index)
    {
        BTreeNode child;
        uint64_t child_offset = parent.child_offsets[child_index];
        if (!read_node(child_offset, child))
            return;

        
        uint64_t new_node_offset = allocate_node();
        BTreeNode new_node;
        new_node.node_type = child.node_type;
        new_node.level = child.level;

        int mid = BTreeNode::MIN_KEYS;

        
        new_node.key_count = BTreeNode::MIN_KEYS;
        for (int i = 0; i < BTreeNode::MIN_KEYS; i++)
        {
            new_node.keys[i] = child.keys[mid + 1 + i];
        }

        
        if (child.is_leaf())
        {
            for (int i = 0; i < BTreeNode::MIN_KEYS; i++)
            {
                new_node.values[i] = child.values[mid + 1 + i];
            }
            
            new_node.next_leaf = child.next_leaf;
            child.next_leaf = new_node_offset;
            new_node.prev_leaf = child_offset;
        }
        else
        {
            for (int i = 0; i <= BTreeNode::MIN_KEYS; i++)
            {
                new_node.child_offsets[i] = child.child_offsets[mid + 1 + i];
            }
        }

        child.key_count = BTreeNode::MIN_KEYS;

        
        for (int i = parent.key_count; i > child_index; i--)
        {
            parent.keys[i] = parent.keys[i - 1];
            parent.child_offsets[i + 1] = parent.child_offsets[i];
        }

        parent.keys[child_index] = child.keys[mid];
        parent.child_offsets[child_index + 1] = new_node_offset;
        parent.key_count++;

        
        write_node(child_offset, child);
        write_node(new_node_offset, new_node);
        write_node(parent_offset, parent);
    }

    
    void insert_non_full(uint64_t node_offset, BTreeNode &node,
                         const CompositeKey &key, const BTreeValue &value)
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
            {
                pos--;
            }
            pos++;

            BTreeNode child;
            uint64_t child_offset = node.child_offsets[pos];
            read_node(child_offset, child);

            if (child.is_full())
            {
                split_child(node_offset, node, pos);
                if (node.keys[pos] < key)
                {
                    pos++;
                }
                read_node(node.child_offsets[pos], child);
            }

            insert_non_full(node.child_offsets[pos], child, key, value);
        }
    }

    
    bool search_recursive(uint64_t node_offset, const CompositeKey &key, BTreeValue &result)
    {
        if (node_offset == 0)
            return false;

        BTreeNode node;
        if (!read_node(node_offset, node))
            return false;

        int pos = find_key_position(node, key);

        
        if (pos < node.key_count && node.keys[pos] == key)
        {
            if (node.is_leaf())
            {
                result = node.values[pos];
                return true;
            }
            else
            {
                
                return search_recursive(node.child_offsets[pos + 1], key, result);
            }
        }

        
        if (node.is_leaf())
        {
            return false;
        }

        
        return search_recursive(node.child_offsets[pos], key, result);
    }

    
    void range_query_recursive(uint64_t node_offset,
                               const CompositeKey &start_key,
                               const CompositeKey &end_key,
                               vector<pair<CompositeKey, BTreeValue>> &results)
    {
        if (node_offset == 0)
            return;

        BTreeNode node;
        if (!read_node(node_offset, node))
            return;

        if (node.is_leaf())
        {
            
            for (int i = 0; i < node.key_count; i++)
            {
                if (node.keys[i] >= start_key && node.keys[i] <= end_key)
                {
                    results.push_back({node.keys[i], node.values[i]});
                }
            }

            
            if (node.key_count > 0 && node.keys[node.key_count - 1] < end_key && node.next_leaf != 0)
            {
                range_query_recursive(node.next_leaf, start_key, end_key, results);
            }
        }
        else
        {
            
            int pos = find_key_position(node, start_key);
            for (int i = pos; i <= node.key_count; i++)
            {
                range_query_recursive(node.child_offsets[i], start_key, end_key, results);
                if (i < node.key_count && node.keys[i] > end_key)
                    break;
            }
        }
    }

public:
    BTree(const string &filename) : filename_(filename)
    {
        cache_.reserve(CACHE_SIZE);
    }

    ~BTree()
    {
        close();
    }
   
    bool create()
    {
        
        if (file_.is_open())
        {
            file_.close();
        }

        cout << "        Opening file for creation: " << filename_ << endl;

        file_.open(filename_, ios::out | ios::binary | ios::trunc);
        if (!file_.is_open())
        {
            cerr << "        ERROR: Cannot create file: " << filename_ << endl;
            cerr << "        Error: " << strerror(errno) << endl;
            return false;
        }

        cout << "        Writing metadata..." << endl;

        
        metadata_ = BTreeMetadata();
        file_.write(reinterpret_cast<char *>(&metadata_), sizeof(BTreeMetadata));

        if (!file_.good())
        {
            cerr << "        ERROR: Failed to write metadata!" << endl;
            file_.close();
            return false;
        }

        cout << "        Creating root node..." << endl;

        
        BTreeNode root;
        root.node_type = 1; 
        root.level = 0;
        metadata_.root_offset = sizeof(BTreeMetadata);

        file_.write(reinterpret_cast<char *>(&root), sizeof(BTreeNode));

        if (!file_.good())
        {
            cerr << "        ERROR: Failed to write root node!" << endl;
            file_.close();
            return false;
        }

        file_.flush();

        cout << "        Updating metadata..." << endl;

        
        file_.seekp(0, ios::beg);
        file_.write(reinterpret_cast<char *>(&metadata_), sizeof(BTreeMetadata));
        file_.flush();


        cout << "        BTree file created successfully" << endl;
        return true;
    }

    bool open()
    {
        if (file_.is_open())
        {
            cout << "        File already open, verifying..." << endl;
            file_.seekg(0, ios::beg);
            file_.read(reinterpret_cast<char *>(&metadata_), sizeof(BTreeMetadata));

            if (string(metadata_.magic, 8) != "BTREE001")
            {
                cerr << "        ERROR: Invalid magic number!" << endl;
                file_.close();
                return false;
            }

            cout << "        Verification successful" << endl;
            return true;
        }

        cout << "        Opening file: " << filename_ << endl;

        file_.open(filename_, ios::in | ios::out | ios::binary);
        if (!file_.is_open())
        {
            cerr << "        ERROR: Cannot open file: " << filename_ << endl;
            return false;
        }

        file_.read(reinterpret_cast<char *>(&metadata_), sizeof(BTreeMetadata));

        if (string(metadata_.magic, 8) != "BTREE001")
        {
            cerr << "        ERROR: Invalid BTree file (bad magic)" << endl;
            file_.close();
            return false;
        }

        cout << "        BTree opened successfully" << endl;
        return true;
    }
    
    void close()
    {
        
        for (auto &entry : cache_)
        {
            if (entry.dirty)
            {
                write_node(entry.offset, entry.node);
            }
        }
        cache_.clear();

        if (file_.is_open())
        {
            
            file_.seekp(0, ios::beg);
            file_.write(reinterpret_cast<char *>(&metadata_), sizeof(BTreeMetadata));
            file_.flush();
            file_.close();
        }
    }

    
    bool insert(const CompositeKey &key, const BTreeValue &value)
    {
        if (metadata_.root_offset == 0)
            return false;

        BTreeNode root;
        read_node(metadata_.root_offset, root);

        if (root.is_full())
        {
            
            uint64_t new_root_offset = allocate_node();
            BTreeNode new_root;
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

        metadata_.total_records++;
        return true;
    }

    bool search(const CompositeKey &key, BTreeValue &result)
    {
        return search_recursive(metadata_.root_offset, key, result);
    }

    vector<pair<CompositeKey, BTreeValue>> range_query(
        const CompositeKey &start_key, const CompositeKey &end_key)
    {

        vector<pair<CompositeKey, BTreeValue>> results;
        range_query_recursive(metadata_.root_offset, start_key, end_key, results);
        return results;
    }

    uint64_t get_total_records() const { return metadata_.total_records; }
    uint32_t get_tree_height() const { return metadata_.tree_height; }
    size_t get_cache_size() const { return cache_.size(); }
};

#endif