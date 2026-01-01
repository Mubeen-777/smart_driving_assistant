
#ifndef DOUBLYLINKEDLIST_H
#define DOUBLYLINKEDLIST_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
using namespace std;

struct ListNode
{
    uint64_t prev_offset;
    uint64_t next_offset;
    uint64_t data_offset;
    uint64_t timestamp;
    uint32_t data_size;
    uint8_t node_type;
    uint8_t reserved[11];

    ListNode() : prev_offset(0), next_offset(0), data_offset(0),
                 timestamp(0), data_size(0), node_type(0)
    {
        memset(reserved, 0, sizeof(reserved));
    }
};

struct ListMetadata
{
    char magic[8];
    uint64_t head_offset;
    uint64_t tail_offset;
    uint64_t node_count;
    uint64_t owner_id;
    uint8_t reserved[4064];

    ListMetadata() : head_offset(0), tail_offset(0), node_count(0), owner_id(0)
    {
        strncpy(magic, "DLIST001", 8);
        memset(reserved, 0, sizeof(reserved));
    }
};

class DoublyLinkedList
{
private:
    fstream file_;
    string filename_;
    ListMetadata metadata_;

    bool read_node(uint64_t offset, ListNode &node)
    {
        if (offset == 0)
            return false;
        file_.seekg(offset, ios::beg);
        file_.read(reinterpret_cast<char *>(&node), sizeof(ListNode));
        return file_.good();
    }

    bool write_node(uint64_t offset, const ListNode &node)
    {
        file_.seekp(offset, ios::beg);
        file_.write(reinterpret_cast<const char *>(&node), sizeof(ListNode));
        file_.flush();
        return file_.good();
    }

    uint64_t allocate_node()
    {
        file_.seekp(0, ios::end);
        return file_.tellp();
    }

public:
    DoublyLinkedList(const string &filename) : filename_(filename) {}

    ~DoublyLinkedList()
    {
        close();
    }

    bool create(uint64_t owner_id)
    {
        file_.open(filename_, ios::out | ios::binary | ios::trunc);
        if (!file_.is_open())
            return false;

        metadata_.owner_id = owner_id;
        file_.write(reinterpret_cast<char *>(&metadata_), sizeof(ListMetadata));
        file_.close();
        return true;
    }

    bool open()
    {
        file_.open(filename_, ios::in | ios::out | ios::binary);
        if (!file_.is_open())
            return false;

        file_.read(reinterpret_cast<char *>(&metadata_), sizeof(ListMetadata));
        return string(metadata_.magic, 8) == "DLIST001";
    }

    void close()
    {
        if (file_.is_open())
        {
            file_.seekp(0);
            file_.write(reinterpret_cast<char *>(&metadata_), sizeof(ListMetadata));
            file_.close();
        }
    }

    bool insert_at_head(uint64_t data_offset, uint32_t data_size,
                        uint8_t node_type, uint64_t timestamp)
    {
        uint64_t new_node_offset = allocate_node();

        ListNode new_node;
        new_node.data_offset = data_offset;
        new_node.data_size = data_size;
        new_node.node_type = node_type;
        new_node.timestamp = timestamp;
        new_node.next_offset = metadata_.head_offset;
        new_node.prev_offset = 0;

        if (metadata_.head_offset != 0)
        {
            ListNode old_head;
            read_node(metadata_.head_offset, old_head);
            old_head.prev_offset = new_node_offset;
            write_node(metadata_.head_offset, old_head);
        }
        else
        {
            metadata_.tail_offset = new_node_offset;
        }

        metadata_.head_offset = new_node_offset;
        metadata_.node_count++;

        return write_node(new_node_offset, new_node);
    }

    bool insert_at_tail(uint64_t data_offset, uint32_t data_size,
                        uint8_t node_type, uint64_t timestamp)
    {
        uint64_t new_node_offset = allocate_node();

        ListNode new_node;
        new_node.data_offset = data_offset;
        new_node.data_size = data_size;
        new_node.node_type = node_type;
        new_node.timestamp = timestamp;
        new_node.prev_offset = metadata_.tail_offset;
        new_node.next_offset = 0;

        if (metadata_.tail_offset != 0)
        {
            ListNode old_tail;
            read_node(metadata_.tail_offset, old_tail);
            old_tail.next_offset = new_node_offset;
            write_node(metadata_.tail_offset, old_tail);
        }
        else
        {
            metadata_.head_offset = new_node_offset;
        }

        metadata_.tail_offset = new_node_offset;
        metadata_.node_count++;

        return write_node(new_node_offset, new_node);
    }

    bool remove_node(uint64_t node_offset)
    {
        ListNode node;
        if (!read_node(node_offset, node))
            return false;

        if (node.prev_offset != 0)
        {
            ListNode prev_node;
            read_node(node.prev_offset, prev_node);
            prev_node.next_offset = node.next_offset;
            write_node(node.prev_offset, prev_node);
        }
        else
        {
            metadata_.head_offset = node.next_offset;
        }

        if (node.next_offset != 0)
        {
            ListNode next_node;
            read_node(node.next_offset, next_node);
            next_node.prev_offset = node.prev_offset;
            write_node(node.next_offset, next_node);
        }
        else
        {
            metadata_.tail_offset = node.prev_offset;
        }

        metadata_.node_count--;
        return true;
    }

    vector<ListNode> get_recent(int count)
    {
        vector<ListNode> results;
        results.reserve(count);

        uint64_t current = metadata_.head_offset;
        int retrieved = 0;

        while (current != 0 && retrieved < count)
        {
            ListNode node;
            if (read_node(current, node))
            {
                results.push_back(node);
                current = node.next_offset;
                retrieved++;
            }
            else
            {
                break;
            }
        }

        return results;
    }

    vector<ListNode> get_range(uint64_t start_time, uint64_t end_time)
    {
        vector<ListNode> results;

        uint64_t current = metadata_.head_offset;

        while (current != 0)
        {
            ListNode node;
            if (!read_node(current, node))
                break;

            if (node.timestamp >= start_time && node.timestamp <= end_time)
            {
                results.push_back(node);
            }

            if (node.timestamp < start_time)
                break;

            current = node.next_offset;
        }

        return results;
    }

    template <typename Func>
    void traverse_forward(Func callback)
    {
        uint64_t current = metadata_.head_offset;

        while (current != 0)
        {
            ListNode node;
            if (!read_node(current, node))
                break;

            if (!callback(node))
                break;
            current = node.next_offset;
        }
    }

    template <typename Func>
    void traverse_backward(Func callback)
    {
        uint64_t current = metadata_.tail_offset;

        while (current != 0)
        {
            ListNode node;
            if (!read_node(current, node))
                break;

            if (!callback(node))
                break;
            current = node.prev_offset;
        }
    }

    uint64_t get_head() const { return metadata_.head_offset; }
    uint64_t get_tail() const { return metadata_.tail_offset; }
    uint64_t get_count() const { return metadata_.node_count; }
    uint64_t get_owner_id() const { return metadata_.owner_id; }
    bool empty() const { return metadata_.node_count == 0; }
};

#endif